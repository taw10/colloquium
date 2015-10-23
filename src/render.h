/*
 * render.h
 *
 * Copyright © 2013-2015 Thomas White <taw@bitwiz.org.uk>
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

#ifndef RENDER_H
#define RENDER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "presentation.h"
#include "imagestore.h"
#include "sc_interp.h"
#include "frame.h"

/* Convienience function to run the entire pipeline */
extern cairo_surface_t *render_sc(SCBlock *scblocks, int w, int h,
                                  double log_w, double log_h,
                                  SCBlock **stylesheets, SCCallbackList *cbl,
                                  ImageStore *is, enum is_size isz,
                                  int slide_number, struct frame **ptop);

/* Interpret StoryCode and measure boxes.
 * Needs to be followed by: wrap_contents() (recursively)
 *                          recursive_draw()
 */
extern struct frame *interp_and_shape(SCBlock *scblocks, SCBlock **stylesheets,
                                      SCCallbackList *cbl,
                                      ImageStore *is, enum is_size isz,
                                      int slide_number, cairo_t *cr,
                                      double w, double h);

extern int recursive_wrap(struct frame *fr, ImageStore *is, enum is_size isz);

extern int export_pdf(struct presentation *p, const char *filename);

extern int recursive_draw(struct frame *fr, cairo_t *cr,
                          ImageStore *is, enum is_size isz,
                          double min_y, double max_y);

#endif	/* RENDER_H */
