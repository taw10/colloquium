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
#include <gio/gio.h>
#include <gdk/gdk.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "slide.h"
#include "slide_pdf.h"
#include "slide_svg.h"
#include "slide_bitmap.h"
#include "render_utils.h"


Slide *slide_new()
{
    Slide *s;
    s = malloc(sizeof(*s));
    if ( s == NULL ) return NULL;
    s->ext_file = NULL;
    s->aspect = -1.0;
    s->file_type = SLIDE_FTYPE_UNKNOWN;
    s->mediastream = NULL;
    s->hide_elements = NULL;
    s->background = NULL;
    return s;
}


void slide_free(Slide *s)
{
    if ( s->ext_file != NULL ) g_object_unref(s->ext_file);
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


void slide_set_background_file(Slide *s, GFile *file)
{
    s->background = g_file_dup(file);
}


void slide_set_ext_number(Slide *s, int num)
{
    s->ext_slidenumber = num;
}


void slide_set_hidden_elements(Slide *s, char **elements, int n)
{
    int i;

    s->hide_elements = malloc((n+1)*sizeof(char *));
    for ( i=0; i<n; i++ ) {
        s->hide_elements[i] = g_strdup(elements[i]);
    }
    s->hide_elements[n] = NULL;
}


static int ensure_ftype(Slide *s)
{
    if ( s->file_type == SLIDE_FTYPE_UNKNOWN ) {
        s->file_type = query_file_type(s->ext_file);
    }

    if ( s->file_type == SLIDE_FTYPE_UNKNOWN ) return 1;
    return 0;
}


enum slide_filetype slide_ftype(Slide *s)
{
    ensure_ftype(s);
    return s->file_type;
}


/* Draw the slide background with width 'w' onto 'cr'.
 * Set up the Cairo context for drawing the slide contents, and return
 * the width at which the contents should be rendered. */
static float setup_background(Slide *s, int w, cairo_t *cr)
{
    double rect[4];
    float sw, xoffs, yoffs;
    double real_aspect;
    int h;

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    char *hide_content[2] = {"content", NULL};
    load_svg(s->background, w, hide_content, cr, rect);

    /* Get the real aspect, not that of the "slide" (which is set by the background) */
    switch ( s->file_type ) {

        case SLIDE_FTYPE_PDF:
        if ( s->ext_slidenumber == 0 ) {
            fprintf(stderr, "PDF without page number\n");
            real_aspect = 1.0;
        }
        real_aspect = get_aspect_pdf(s->ext_file, s->ext_slidenumber);
        break;

        case SLIDE_FTYPE_IMAGE:
        real_aspect = get_aspect_image(s->ext_file);
        break;

        case SLIDE_FTYPE_SVG:
        real_aspect = get_aspect_svg(s->ext_file);
        break;

        default:
        fprintf(stderr, _("Unrecognised slide type in setup_background\n"));
        real_aspect = 1.0;
        break;

    }

    h = w/get_aspect_svg(s->background);
    letterbox(w*rect[2], h*rect[3], real_aspect, &sw, &xoffs, &yoffs);
    cairo_translate(cr, w*rect[0], h*rect[1]);
    cairo_translate(cr, xoffs, yoffs);
    return sw;
}


static void draw_slide_without_background(Slide *s, int w, cairo_t *cr)
{
    switch ( s->file_type ) {

        case SLIDE_FTYPE_PDF:
        if ( s->ext_slidenumber == 0 ) {
            fprintf(stderr, "PDF without page number\n");
            return;
        }
        load_pdf(s->ext_file, s->ext_slidenumber, w, cr);
        break;

        case SLIDE_FTYPE_IMAGE:
        load_image_cairo(s->ext_file, w, cr);
        break;

        case SLIDE_FTYPE_SVG:
        load_svg(s->ext_file, w, s->hide_elements, cr, NULL);
        break;

        case SLIDE_FTYPE_VIDEO:
        fprintf(stderr, _("Video cannot be rendered this way.\n"));
        return;

        default:
        fprintf(stderr, _("Unrecognised file type (Cairo): %i\n"), s->file_type);
        return;
    }
}


static GdkPaintable *slide_render_with_background(Slide *s, int w)
{
    cairo_surface_t *surf;
    cairo_t *cr;
    float sw;
    int h;

    h = w/get_aspect_svg(s->background);
    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surf);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    sw = setup_background(s, w, cr);
    draw_slide_without_background(s, sw, cr);

    cairo_destroy(cr);

    return GDK_PAINTABLE(surface_to_paintable(surf, w, h));
}


GdkPaintable *slide_render(Slide *s, int w)
{
    if ( ensure_ftype(s) ) return placeholder_image();

    if ( s->background != NULL ) {
        if ( s->file_type == SLIDE_FTYPE_VIDEO ) {
            fprintf(stderr, _("Cannot render video with background\n"));
            if ( s->mediastream == NULL ) {
                s->mediastream = gtk_media_file_new_for_file(s->ext_file);
            }
            return GDK_PAINTABLE(s->mediastream);
        } else {
            return GDK_PAINTABLE(slide_render_with_background(s, w));
        }
    }

    switch ( s->file_type ) {

        case SLIDE_FTYPE_PDF:
        if ( s->ext_slidenumber == 0 ) {
            fprintf(stderr, "PDF without page number\n");
            return placeholder_image();
        }
        return GDK_PAINTABLE(load_pdf(s->ext_file, s->ext_slidenumber, w, NULL));

        case SLIDE_FTYPE_IMAGE:
        return GDK_PAINTABLE(load_image(s->ext_file, w));

        case SLIDE_FTYPE_SVG:
        return GDK_PAINTABLE(load_svg(s->ext_file, w, s->hide_elements, NULL, NULL));

        case SLIDE_FTYPE_VIDEO:
        if ( s->mediastream == NULL ) {
            s->mediastream = gtk_media_file_new_for_file(s->ext_file);
        }
        return GDK_PAINTABLE(s->mediastream);

        default:
        fprintf(stderr, _("Unrecognised file type (paintable): %i\n"), s->file_type);
        return NULL;
    }
}


void slide_render_cairo(Slide *s, int w, cairo_t *cr)
{
    if ( ensure_ftype(s) ) return;

    if ( s->background != NULL ) {
        if ( s->file_type == SLIDE_FTYPE_VIDEO ) {
            fprintf(stderr, _("Cannot render video with background\n"));
            return;
        } else {
            cairo_save(cr);
            w = setup_background(s, w, cr);
        }
    }

    draw_slide_without_background(s, w, cr);

    if ( s->background != NULL ) {
        cairo_restore(cr);
    }
}


/* Returns the aspect ratio of the slide, including backgrounds, headers etc */
float slide_get_aspect(Slide *s)
{
    float t;

    if ( s->aspect > 0 ) return s->aspect;

    if ( s->background != NULL ) {
        s->aspect = get_aspect_svg(s->background);
        return s->aspect;
    }

    if ( ensure_ftype(s) ) return 1.0;

    switch ( s->file_type ) {

        case SLIDE_FTYPE_PDF:
        if ( s->ext_slidenumber == 0 ) {
            fprintf(stderr, "PDF without page number\n");
            return 1.0;
        }
        s->aspect = get_aspect_pdf(s->ext_file, s->ext_slidenumber);
        break;

        case SLIDE_FTYPE_IMAGE:
        s->aspect = get_aspect_image(s->ext_file);
        break;

        case SLIDE_FTYPE_SVG:
        s->aspect = get_aspect_svg(s->ext_file);
        break;

        case SLIDE_FTYPE_VIDEO:
        if ( s->mediastream == NULL ) {
            s->mediastream = GTK_MEDIA_STREAM(gtk_media_file_new_for_file(s->ext_file));
        }
        if ( !gtk_media_stream_is_prepared(s->mediastream) ) {
            return 1.0;  /* but don't set s->aspect */
        }
        t = gdk_paintable_get_intrinsic_aspect_ratio(GDK_PAINTABLE(s->mediastream));
        if ( t == 0.0 ) {
            return 1.0;  /* but don't set s->aspect */
        }
        s->aspect = t;
        break;

        default:
        fprintf(stderr, _("Unrecognised file type (aspect): %i\n"), s->file_type);
        return 1.0;
    }

    return s->aspect;
}
