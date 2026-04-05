/*
 * slide.c
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
 *
 * This file is part of Colloquium.
 *
 * Colloquium is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <cairo.h>
#include <poppler.h>
#include <librsvg/rsvg.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "slide.h"


Slide *slide_new()
{
    Slide *s;
    s = malloc(sizeof(*s));
    if ( s == NULL ) return NULL;
    s->ext_file = NULL;
    s->aspect = -1.0;
    s->file_type = SLIDE_FTYPE_UNKNOWN;
    s->paintable = NULL;
    return s;
}


void slide_free(Slide *s)
{
    if ( s->ext_file != NULL ) g_object_unref(s->ext_file);
    if ( s->paintable != NULL ) g_object_unref(s->paintable);
    free(s);
}


Slide *slide_copy(const Slide *s)
{
    Slide *o = slide_new();
    *o = *s;
    o->anchor = NULL;
    if ( s->ext_file != NULL ) {
        o->ext_file = g_file_dup(o->ext_file);
    } else {
        o->ext_file = NULL;
    }
    return o;
}


void slide_set_ext_file(Slide *s, GFile *file)
{
    s->ext_file = g_file_dup(file);
}


void slide_set_ext_number(Slide *s, int num)
{
    s->ext_slidenumber = num;
}


#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define MEMFORMAT GDK_MEMORY_B8G8R8X8
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define MEMFORMAT GDK_MEMORY_X8R8G8B8
#else
#define MEMFORMAT "unknown byte order"
#endif


static GdkTexture *surface_to_paintable(cairo_surface_t *surf, int w, int h)
{
    GBytes *bytes;
    GdkTexture *tex;

    bytes = g_bytes_new_with_free_func(cairo_image_surface_get_data(surf),
                                       h*cairo_image_surface_get_stride(surf),
                                       (GDestroyNotify)cairo_surface_destroy,
                                       cairo_surface_reference(surf));

    tex = gdk_memory_texture_new(w, h, MEMFORMAT, bytes,
                                 cairo_image_surface_get_stride(surf));

    g_bytes_unref(bytes);
    cairo_surface_destroy(surf);

    return tex;
}


static GdkTexture *load_image(GFile *file)
{
    GFileInputStream *stream;
    GError *error;
    GdkPixbuf *pixbuf;

    error = NULL;
    stream = g_file_read(file, NULL, &error);
    if ( stream == NULL ) {
        fprintf(stderr, _("Failed to read image: %s\n"), error->message);
        return NULL;
    }

    error = NULL;
    pixbuf = gdk_pixbuf_new_from_stream_at_scale(G_INPUT_STREAM(stream),
                                                 -1, -1, TRUE, NULL, &error);
    g_object_unref(G_OBJECT(stream));
    if ( pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load image (paintable): %s\n"), error->message);
        return NULL;
    }

    return gdk_texture_new_for_pixbuf(pixbuf);
}


GdkTexture *load_svg(GFile *file)
{
    RsvgHandle *fh;
    GError *error;
    RsvgLength width, height;
    RsvgRectangle viewbox;
    gboolean has_viewbox, has_width, has_height;
    RsvgRectangle viewport;
    float aspect;
    int w, h;
    cairo_surface_t *surf;
    cairo_t *cr;

    error = NULL;
    fh = rsvg_handle_new_from_gfile_sync(file, RSVG_HANDLE_FLAGS_NONE,
                                        NULL, &error);
    if ( fh == NULL ) {
        fprintf(stderr, _("Failed to read SVG: %s\n"), error->message);
        return NULL;
    }

    rsvg_handle_get_intrinsic_dimensions(fh, &has_width, &width,
                                         &has_height, &height,
                                         &has_viewbox, &viewbox);

    if ( has_viewbox ) {
        aspect = viewbox.width/viewbox.height;
    } else {
        if ( !has_width || !has_height ) {
            fprintf(stderr, _("Failed to load SVG - no width/height\n"));
            g_object_unref(fh);
            return NULL;
        }
        if ( width.unit != height.unit ) {
            fprintf(stderr, _("Failed to load SVG - units not the same\n"));
            g_object_unref(fh);
            return NULL;
        }
        if ( width.unit == RSVG_UNIT_PERCENT ) {
            fprintf(stderr, _("Failed to load SVG - no viewbox and percent size\n"));
            g_object_unref(fh);
            return NULL;
        }
        aspect = width.length / height.length;
    }

    w = 1024;
    h = w/aspect;
    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surf);

    viewport.x = 0;
    viewport.y = 0;
    viewport.width = w;
    viewport.height = h;
    error = NULL;
    rsvg_handle_render_document(fh, cr, &viewport, &error);
    g_object_unref(fh);

    return surface_to_paintable(surf, w, h);
}


GdkTexture *load_pdf(GFile *file, int pagenum)
{
    PopplerDocument *doc;
    PopplerPage *page;
    double pw, ph;
    cairo_surface_t *surf;
    cairo_t *cr;
    int w, h;

    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return NULL;

    page = poppler_document_get_page(doc, pagenum-1);
    if ( page == NULL ) {
        g_object_unref(G_OBJECT(doc));
        return NULL;
    }

    poppler_page_get_size(page, &pw, &ph);

    w = 1024;
    h = w * ph/pw;
    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surf);
    cairo_scale(cr, w/pw, w/pw);
    poppler_page_render(page, cr);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(doc));

    return surface_to_paintable(surf, w, h);
}


static int ensure_ftype(Slide *s)
{
    if ( s->file_type == SLIDE_FTYPE_UNKNOWN ) {

        GFileInfo *info;
        const char *type;
        GError *error;

        if ( s->ext_file == NULL ) return 1;

        error = NULL;
        info = g_file_query_info(s->ext_file, "standard::", G_FILE_QUERY_INFO_NONE, NULL, &error);
        if ( info == NULL ) {
            fprintf(stderr, _("Failed to read info: %s\n"), error->message);
            return 1;
        }

        type = g_file_info_get_content_type(info);
        if ( g_content_type_equals(type, "application/pdf") ) {
            s->file_type = SLIDE_FTYPE_PDF;
        } else if ( g_content_type_equals(type, "image/png") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else if ( g_content_type_equals(type, "com.adobe.pdf") ) {
            s->file_type = SLIDE_FTYPE_PDF;
        } else if ( g_content_type_equals(type, "public.png") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else if ( g_content_type_equals(type, "image/jpeg") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else if ( g_content_type_equals(type, "public.jpeg") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else if ( g_content_type_equals(type, "image/svg+xml") ) {
            s->file_type = SLIDE_FTYPE_SVG;
        } else if ( g_content_type_equals(type, "public.svg-image") ) {
            s->file_type = SLIDE_FTYPE_SVG;
        } else if ( g_content_type_equals(type, "video/mpeg") ) {
            s->file_type = SLIDE_FTYPE_VIDEO;
        } else {
            fprintf(stderr, "File format not recognised: %s\n", type);
            s->file_type = SLIDE_FTYPE_UNKNOWN;
        }

        g_object_unref(G_OBJECT(info));

    }

    if ( s->file_type == SLIDE_FTYPE_UNKNOWN ) return 1;
    return 0;
}


static void ensure_paintable(Slide *s)
{
    if ( s->paintable != NULL ) return;

    if ( ensure_ftype(s) ) return;

    switch ( s->file_type ) {

        case SLIDE_FTYPE_PDF:
        s->paintable = GDK_PAINTABLE(load_pdf(s->ext_file, s->ext_slidenumber));
        break;

        case SLIDE_FTYPE_IMAGE:
        s->paintable = GDK_PAINTABLE(load_image(s->ext_file));
        break;

        case SLIDE_FTYPE_SVG:
        s->paintable = GDK_PAINTABLE(load_svg(s->ext_file));
        break;

        default:
        fprintf(stderr, _("Unrecognised ile type (paintable): %i\n"), s->file_type);
        return;
    }
}


GdkPaintable *slide_get_paintable(Slide *s)
{
    ensure_paintable(s);
    return s->paintable;
}


float slide_get_aspect(Slide *s)
{
    ensure_paintable(s);
    return gdk_paintable_get_intrinsic_aspect_ratio(s->paintable);
}


void letterbox(float dw, float dh, float aspect,
               float *sw, float *xoff, float *yoff)
{
    if ( aspect > dw/dh ) {
        /* Slide is too wide.  Letterboxing top/bottom */
        *sw = dw;
    } else {
        /* Letterboxing at sides */
        *sw = dh * aspect;
    }

    *xoff = (dw - (*sw))/2.0;
    *yoff = (dh - (*sw)/aspect)/2.0;
}
