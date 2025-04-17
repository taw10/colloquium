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
    s->logical_w = -1.0;
    s->logical_h = -1.0;
    s->ext_filename = NULL;
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


int slide_set_logical_size(Slide *s, double w, double h)
{
    if ( s == NULL ) return 1;
    s->logical_w = w;
    s->logical_h = h;
    return 0;
}


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


int slide_get_logical_size(Slide *s, double *w, double *h)
{
    if ( s == NULL ) return 1;

    /* Slide-specific value not set */
    if ( s->logical_w < 0.0 ) return 1;

    *w = s->logical_w;
    *h = s->logical_h;
    return 0;
}
