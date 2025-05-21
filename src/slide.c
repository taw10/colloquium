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

#include <libintl.h>
#define _(x) gettext(x)

#include "slide.h"


Slide *slide_new()
{
    Slide *s;
    s = malloc(sizeof(*s));
    if ( s == NULL ) return NULL;
    s->ext_filename = NULL;
    s->aspect = -1.0;
    s->file_type = SLIDE_FTYPE_UNKNOWN;
    return s;
}

void slide_free(Slide *s)
{
    free(s);
}


Slide *slide_copy(const Slide *s)
{
    Slide *o = slide_new();
    *o = *s;
    o->anchor = NULL;
    o->ext_filename = strdup(s->ext_filename);
    return o;
}


void slide_set_ext_filename(Slide *s, char *filename)
{
    s->ext_filename = filename;
}


void slide_set_ext_number(Slide *s, int num)
{
    s->ext_slidenumber = num;
}


static float get_aspect_image(Slide *s)
{
    GFile *file;
    GFileInputStream *stream;
    GError *error;
    GdkPixbuf *pixbuf;
    int pw, ph;

    file = g_file_new_for_path(s->ext_filename);
    error = NULL;
    stream = g_file_read(file, NULL, &error);
    g_object_unref(file);
    if ( stream == NULL ) {
        fprintf(stderr, _("Failed to open read (aspect): %s\n"), error->message);
        return 1;
    }

    error = NULL;
    pixbuf = gdk_pixbuf_new_from_stream(G_INPUT_STREAM(stream), NULL, &error);
    g_object_unref(stream);
    if ( pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load image (aspect): %s\n"), error->message);
        return 1;
    }

    pw = gdk_pixbuf_get_width(pixbuf);
    ph = gdk_pixbuf_get_height(pixbuf);
    g_object_unref(pixbuf);

    return (float)pw/ph;
}


static float get_aspect_pdf(Slide *s)
{
    GFile *file;
    PopplerDocument *doc;
    PopplerPage *page;
    double pw, ph;

    file = g_file_new_for_path(s->ext_filename);
    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    g_object_unref(G_OBJECT(file));
    if ( doc == NULL ) return 1;

    page = poppler_document_get_page(doc, s->ext_slidenumber-1);
    if ( page == NULL ) {
        g_object_unref(G_OBJECT(doc));
        return 1;
    }

    poppler_page_get_size(page, &pw, &ph);
    g_object_unref(G_OBJECT(doc));
    g_object_unref(G_OBJECT(page));
    return pw/ph;
}


static int render_cairo_image(Slide *s, cairo_t *cr, float w)
{
    GFileInputStream *stream;
    GError *error;
    GdkPixbuf *pixbuf;
    int pw;
    double scale;
    GFile *file;

    file = g_file_new_for_path(s->ext_filename);
    error = NULL;
    stream = g_file_read(file, NULL, &error);
    g_object_unref(file);
    if ( stream == NULL ) {
        fprintf(stderr, _("Failed to read image: %s\n"), error->message);
        return 1;
    }

    error = NULL;
    pixbuf = gdk_pixbuf_new_from_stream(G_INPUT_STREAM(stream), NULL, &error);
    g_object_unref(G_OBJECT(stream));
    if ( pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load image: %s\n"), error->message);
        return 1;
    }

    pw = gdk_pixbuf_get_width(pixbuf);
    scale = w/pw;

    cairo_save(cr);
    cairo_scale(cr, scale, scale);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_pattern_t *patt = cairo_get_source(cr);
    cairo_pattern_set_extend(patt, CAIRO_EXTEND_PAD);
    cairo_pattern_set_filter(patt, CAIRO_FILTER_BEST);
    cairo_paint(cr);
    cairo_restore(cr);

    g_object_unref(G_OBJECT(pixbuf));

    return 0;
}


static int render_cairo_pdf(Slide *s, cairo_t *cr, float w)
{
    PopplerDocument *doc;
    PopplerPage *page;
    double pw, ph;
    GFile *file;

    file = g_file_new_for_path(s->ext_filename);
    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    g_object_unref(G_OBJECT(file));
    if ( doc == NULL ) return 1;

    page = poppler_document_get_page(doc, s->ext_slidenumber-1);
    if ( page == NULL ) {
        g_object_unref(G_OBJECT(doc));
        return 1;
    }

    poppler_page_get_size(page, &pw, &ph);
    cairo_save(cr);
    cairo_scale(cr, w/pw, w/pw);
    poppler_page_render(page, cr);
    cairo_restore(cr);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(doc));

    return 0;
}


static int ensure_ftype(Slide *s)
{
    if ( s->file_type == SLIDE_FTYPE_UNKNOWN ) {

        GFile *file;
        GFileInfo *info;
        const char *type;
        GError *error;

        file = g_file_new_for_path(s->ext_filename);
        error = NULL;
        info = g_file_query_info(file, "standard::", G_FILE_QUERY_INFO_NONE, NULL, &error);
        g_object_unref(G_OBJECT(file));
        if ( info == NULL ) {
            fprintf(stderr, _("Failed to read info: %s\n"), error->message);
            return 1;
        }

        type = g_file_info_get_content_type(info);
        if ( g_content_type_equals(type, "application/pdf") ) {
            s->file_type = SLIDE_FTYPE_PDF;
        } else if ( g_content_type_equals(type, "image/png") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else if ( g_content_type_equals(type, "image/jpeg") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else if ( g_content_type_equals(type, "image/svg+xml") ) {
            s->file_type = SLIDE_FTYPE_IMAGE;
        } else {
            fprintf(stderr, "File format not recognised: %s\n", type);
            s->file_type = SLIDE_FTYPE_UNKNOWN;
        }

        g_object_unref(G_OBJECT(info));

    }

    if ( s->file_type == SLIDE_FTYPE_UNKNOWN ) return 1;
    return 0;
}


float slide_get_aspect(Slide *s)
{
    if ( s == NULL ) return 1.0;

    if ( s->aspect < 0.0 ) {
        if ( ensure_ftype(s) ) return 1.0;
        switch ( s->file_type ) {

            case SLIDE_FTYPE_PDF:
            s->aspect = get_aspect_pdf(s);
            return s->aspect;

            case SLIDE_FTYPE_IMAGE:
            s->aspect = get_aspect_image(s);
            return s->aspect;

            default:
            return 1.0; /* Don't know! */
        }
    } else {
        return s->aspect;
    }
}


int slide_render_cairo(Slide *s, cairo_t *cr, float w)
{
    if ( ensure_ftype(s) ) return 1;

    switch ( s->file_type ) {

        case SLIDE_FTYPE_PDF:
        return render_cairo_pdf(s, cr, w);

        case SLIDE_FTYPE_IMAGE:
        return render_cairo_image(s, cr, w);

        default:
        return 1;
    }
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
