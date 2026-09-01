/*
 * slide_pdf.c
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
#include <cairo.h>
#include <poppler.h>

#include "render_utils.h"

float get_aspect_pdf(GFile *file, int pagenum)
{
    PopplerDocument *doc;
    PopplerPage *page;
    double pw, ph;

    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return 1.0;

    page = poppler_document_get_page(doc, pagenum-1);
    if ( page == NULL ) {
        g_object_unref(G_OBJECT(doc));
        return 1.0;
    }

    poppler_page_get_size(page, &pw, &ph);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(doc));

    return pw/ph;
}


GdkTexture *load_pdf(GFile *file, int pagenum, int w, cairo_t *in_cr)
{
    PopplerDocument *doc;
    PopplerPage *page;
    double pw, ph;
    cairo_surface_t *surf;
    cairo_t *cr;
    int h;

    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return NULL;

    page = poppler_document_get_page(doc, pagenum-1);
    if ( page == NULL ) {
        g_object_unref(G_OBJECT(doc));
        return NULL;
    }

    poppler_page_get_size(page, &pw, &ph);

    if ( in_cr == NULL ) {
        h = w * ph/pw;
        surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
        cr = cairo_create(surf);
    } else {
        cr = in_cr;
    }

    cairo_scale(cr, w/pw, w/pw);
    poppler_page_render(page, cr);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(doc));

    if ( in_cr == NULL ) {
        return surface_to_paintable(surf, w, h);
    } else {
        return NULL;
    }
}

