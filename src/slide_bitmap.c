/*
 * slide_bitmap.c
 *
 * Copyright © 2016-2026 Thomas White <taw@bitwiz.org.uk>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libintl.h>
#define _(x) gettext(x)

#include <gio/gio.h>
#include <gio/gio.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "render_utils.h"


float get_aspect_image(GFile *file)
{
    GFileInputStream *stream;
    GError *error;
    GdkPixbuf *pixbuf;
    int pw, ph;

    error = NULL;
    stream = g_file_read(file, NULL, &error);
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


GdkTexture *load_image(GFile *file, int w)
{
    GFileInputStream *stream;
    GError *error;
    GdkPixbuf *pixbuf;
    GdkPixbuf *withbg;

    error = NULL;
    stream = g_file_read(file, NULL, &error);
    if ( stream == NULL ) {
        fprintf(stderr, _("Failed to read image: %s\n"), error->message);
        return NULL;
    }

    error = NULL;
    pixbuf = gdk_pixbuf_new_from_stream_at_scale(G_INPUT_STREAM(stream),
                                                 w, -1, TRUE, NULL, &error);
    g_object_unref(G_OBJECT(stream));
    if ( pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load image (paintable): %s\n"), error->message);
        return NULL;
    }

    withbg = gdk_pixbuf_composite_color_simple(pixbuf,
                                               gdk_pixbuf_get_width(pixbuf),
                                               gdk_pixbuf_get_height(pixbuf),
                                               GDK_INTERP_NEAREST, 255, 64,
                                               0xffffffff, 0xffffffff);
    g_object_unref(G_OBJECT(pixbuf));

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    return gdk_texture_new_for_pixbuf(withbg);
    G_GNUC_END_IGNORE_DEPRECATIONS
}


void load_image_cairo(GFile *file, int w, cairo_t *cr)
{
    GFileInputStream *stream;
    GError *error;
    GdkPixbuf *pixbuf;

    error = NULL;
    stream = g_file_read(file, NULL, &error);
    if ( stream == NULL ) {
        fprintf(stderr, _("Failed to read image: %s\n"), error->message);
        return;
    }

    error = NULL;
    pixbuf = gdk_pixbuf_new_from_stream_at_scale(G_INPUT_STREAM(stream),
                                                 w, -1, TRUE, NULL, &error);
    g_object_unref(G_OBJECT(stream));
    if ( pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load image (paintable): %s\n"), error->message);
        return;
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    G_GNUC_END_IGNORE_DEPRECATIONS

    cairo_paint(cr);

    g_object_unref(G_OBJECT(pixbuf));
}


