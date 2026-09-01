/*
 * slide_svg.c
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
#include <gdk/gdk.h>
#include <cairo.h>
#include <librsvg/rsvg.h>

#include "render_utils.h"


float get_aspect_svg(GFile *file)
{
    RsvgHandle *fh;
    GError *error;
    RsvgLength width, height;
    RsvgRectangle viewbox;
    gboolean has_viewbox, has_width, has_height;
    float aspect;

    error = NULL;
    fh = rsvg_handle_new_from_gfile_sync(file, RSVG_HANDLE_FLAGS_NONE,
                                        NULL, &error);
    if ( fh == NULL ) {
        fprintf(stderr, _("Failed to read SVG (aspect): %s\n"), error->message);
        return 1.0;
    }

    rsvg_handle_set_dpi(fh, 96);
    rsvg_handle_get_intrinsic_dimensions(fh, &has_width, &width,
                                         &has_height, &height,
                                         &has_viewbox, &viewbox);

    if ( has_viewbox ) {
        aspect = viewbox.width/viewbox.height;
    } else {
        if ( !has_width || !has_height ) {
            fprintf(stderr, _("Failed to load SVG - no width/height\n"));
            aspect = 1.0;
        }
        if ( width.unit != height.unit ) {
            fprintf(stderr, _("Failed to load SVG - units not the same\n"));
            aspect = 1.0;
        }
        if ( width.unit == RSVG_UNIT_PERCENT ) {
            fprintf(stderr, _("Failed to load SVG - no viewbox and percent size\n"));
            aspect = 1.0;
        }
        aspect = width.length / height.length;
    }

    g_object_unref(fh);
    return aspect;
}


static GdkTexture *load_svg_stream(GInputStream *stream, GFile *file, int w, char **hide_elements,
                                   cairo_t *in_cr)
{
    RsvgHandle *fh;
    GError *error;
    RsvgLength width, height;
    RsvgRectangle viewbox;
    gboolean has_viewbox, has_width, has_height;
    RsvgRectangle viewport;
    float aspect;
    int h;
    cairo_surface_t *surf;
    cairo_t *cr;

    error = NULL;
    fh = rsvg_handle_new_from_stream_sync(stream, file, RSVG_HANDLE_FLAGS_NONE,
                                          NULL, &error);
    if ( fh == NULL ) {
        fprintf(stderr, _("Failed to read SVG: %s\n"), error->message);
        return NULL;
    }

    rsvg_handle_set_dpi(fh, 96);
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
    h = w/aspect;

    if ( in_cr == NULL ) {
        surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
        cr = cairo_create(surf);
    } else {
        cr = in_cr;
    }

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    if ( hide_elements != NULL ) {
        gchar *css = g_strdup("");
        int i = 0;
        while ( hide_elements[i] != NULL ) {
            gchar *ncss = g_strconcat(css, "#", hide_elements[i], "{opacity: 0.0;}", NULL);
            g_free(css);
            css = ncss;
            i++;
        }
        error = NULL;
        if ( !rsvg_handle_set_stylesheet(fh, (const guint8 *)css, strlen(css), &error) ) {
            fprintf(stderr, "CSS error: %s\n", error->message);
        }
        g_free(css);
    }

    viewport.x = 0;
    viewport.y = 0;
    viewport.width = w;
    viewport.height = h;
    error = NULL;
    rsvg_handle_render_document(fh, cr, &viewport, &error);
    g_object_unref(fh);

    if ( in_cr == NULL ) {
        return surface_to_paintable(surf, w, h);
    } else {
        return NULL;
    }
}


GdkTexture *load_svg(GFile *file, int w, char **hide_elements, cairo_t *cr)
{
    GInputStream *stream;
    GError *error = NULL;
    stream = G_INPUT_STREAM(g_file_read(file, NULL, &error));
    if ( stream == NULL ) {
        fprintf(stderr, _("Failed to open SVG: %s\n"), error->message);
        return NULL;
    }
    return load_svg_stream(stream, file, w, hide_elements, cr);
}


GdkPaintable *the_placeholder = NULL;

GdkPaintable *placeholder_image()
{
    GInputStream *stream;
    GError *error = NULL;

    if ( the_placeholder != NULL ) return the_placeholder;

    stream = g_resources_open_stream("/uk/me/bitwiz/colloquium/uk.me.bitwiz.colloquium.svg",
                                     G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
    the_placeholder = GDK_PAINTABLE(load_svg_stream(stream, NULL, 512, NULL, NULL));
    return the_placeholder;
}
