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


GdkPaintable *slide_render(Slide *s, int w)
{
    if ( ensure_ftype(s) ) return placeholder_image();

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
        return GDK_PAINTABLE(load_svg(s->ext_file, w, s->hide_elements, NULL));

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
        load_svg(s->ext_file, w, s->hide_elements, cr);
        break;

        case SLIDE_FTYPE_VIDEO:
        return;

        default:
        fprintf(stderr, _("Unrecognised file type (Cairo): %i\n"), s->file_type);
        return;
    }
}


float slide_get_aspect(Slide *s)
{
    float t;

    if ( s->aspect > 0 ) return s->aspect;

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
