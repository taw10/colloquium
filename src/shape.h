/*
 * shape.h
 *
 * Copyright © 2014-2015 Thomas White <taw@bitwiz.org.uk>
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

extern void shape_box(struct wrap_box *box);

extern int split_words(struct boxvec *boxes, PangoContext *pc,
                       SCBlock *bl, PangoLanguage *lang,
                       int editable, SCInterpreter *scin);

extern int itemize_and_shape(struct wrap_box *box, PangoContext *pc);

extern void add_image_box(struct boxvec *line, const char *filename,
                          int w, int h, int editable);

extern void add_callback_box(struct boxvec *boxes, double w, double h,
                             SCCallbackDrawFunc draw_func,
                             SCCallbackClickFunc click_func,
                             void *bvp, void *vp);

#endif	/* SHAPE_H */
