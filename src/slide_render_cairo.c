/*
 * render.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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

#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <poppler.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "slide.h"
#include "narrative.h"
#include "slide_render_cairo.h"

int slide_render_cairo(Slide *s, cairo_t *cr)
{
    double w, h;
    GFile *file;
    PopplerDocument *doc;
    PopplerPage *page;
    double pw, ph;

    slide_get_logical_size(s, &w, &h);

    file = g_file_new_for_path(s->ext_filename);
    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return 1;

    page = poppler_document_get_page(doc, s->ext_slidenumber-1);
    if ( page == NULL ) return 1;

    poppler_page_get_size(page, &pw, &ph);
    cairo_scale(cr, (double)w/pw, (double)h/ph);
    poppler_page_render(page, cr);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(doc));

    return 0;
}


int render_slides_to_pdf(Narrative *n, const char *filename)
{
    double w = 2048.0;
    cairo_surface_t *surf;
    cairo_t *cr;
    //int i;

    surf = cairo_pdf_surface_create(filename, w, w);
    if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
        fprintf(stderr, _("Couldn't create Cairo surface\n"));
        return 1;
    }

    cr = cairo_create(surf);

    /* FIXME: For each slide */
#if 0
    for ( i=0; i<narrative_get_num_slides(n); i++ )
    {
        Slide *s;
        double log_w, log_h;

        s = narrative_get_slide_by_number(n, i);
        slide_get_logical_size(s, &log_w, &log_h);

        cairo_pdf_surface_set_size(surf, w, w*(log_h/log_w));

        cairo_save(cr);
        cairo_scale(cr, w/log_w, w/log_w);
        slide_render_cairo(s, cr);
        cairo_show_page(cr);
        cairo_restore(cr);
    }
#endif

    cairo_surface_finish(surf);
    cairo_destroy(cr);

    return 0;
}
