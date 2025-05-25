/*
 * slide.h
 *
 * Copyright © 2019-2025 Thomas White <taw@bitwiz.me.uk>
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

enum slide_filetype
{
    SLIDE_FTYPE_UNKNOWN,
    SLIDE_FTYPE_IMAGE,
    SLIDE_FTYPE_PDF
};


struct _slide
{
    float aspect;  /* Width divided by height */
    char *ext_filename;
    int ext_slidenumber;
    GtkTextChildAnchor *anchor;
    enum slide_filetype file_type;
};

typedef struct _slide Slide;


extern Slide *slide_new(void);
extern Slide *slide_copy(const Slide *s);
extern void slide_free(Slide *s);

extern void slide_set_ext_filename(Slide *s, char *filename);
extern void slide_set_ext_number(Slide *s, int num);

extern int slide_set_aspect(Slide *s, float aspect);
extern float slide_get_aspect(Slide *s);
extern int slide_render_cairo(Slide *s, cairo_t *cr, float w);

extern void letterbox(float dw, float dh, float aspect,
                      float *sw, float *xoff, float *yoff);

#endif /* SLIDE_H */
