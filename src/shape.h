/*
 * shape.h
 *
 * Copyright © 2014 Thomas White <taw@bitwiz.org.uk>
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

#ifndef SHAPE_H
#define SHAPE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pango/pangocairo.h>

#include "wrap.h"

extern int split_words(struct wrap_line *boxes, PangoContext *pc,
                       SCBlock *bl, const char *text, PangoLanguage *lang,
                       int editable, SCInterpreter *scin);

extern void add_image_box(struct wrap_line *line, const char *filename,
                          int w, int h, int editable);

extern void add_surface_box(struct wrap_line *line, cairo_surface_t *surf,
                            double w, double h);

extern void reshape_box(struct wrap_box *box);

#endif	/* SHAPE_H */
