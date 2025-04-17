/*
 * slide.h
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

#ifndef SLIDE_H
#define SLIDE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>
#include <gtk/gtk.h>

struct _slide
{
    double logical_w;
    double logical_h;
    char *ext_filename;
    int ext_slidenumber;
    GtkTextChildAnchor *anchor;
};

typedef struct _slide Slide;


extern Slide *slide_new(void);
extern Slide *slide_copy(const Slide *s);
extern void slide_free(Slide *s);

extern void slide_set_ext_filename(Slide *s, char *filename);
extern void slide_set_ext_number(Slide *s, int num);

extern int slide_set_logical_size(Slide *s, double w, double h);
extern int slide_get_logical_size(Slide *s, double *w, double *h);
extern int slide_render_cairo(Slide *s, cairo_t *cr);

#endif /* SLIDE_H */
