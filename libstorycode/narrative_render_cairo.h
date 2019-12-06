/*
 * narrative_render_cairo.h
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

#ifndef NARRATIVE_RENDER_CAIRO_H
#define NARRATIVE_RENDER_CAIRO_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "narrative.h"
#include "imagestore.h"

struct edit_pos
{
	int para;    /* Paragraph number (corresponding to narrative items) */
	int pos;     /* Byte position within paragraph (yes, really)  */
	int trail;   /* 1 = end of character, 0 = before */
};

extern int narrative_wrap_range(Narrative *n, Stylesheet *stylesheet,
                                PangoLanguage *lang, PangoContext *pc, double w,
                                ImageStore *is, int min, int max,
                                struct edit_pos sel_start, struct edit_pos sel_end);

extern double narrative_get_height(Narrative *n);

extern double narrative_item_get_height(Narrative *n, int i);

extern size_t narrative_pos_trail_to_offset(Narrative *n, int i, int offs, int trail);

extern int narrative_render_item_cairo(Narrative*n, cairo_t *cr, int i);

extern int narrative_render_cairo(Narrative *n, cairo_t *cr, Stylesheet *stylesheet, ImageStore *is,
                                  double min_y, double max_y);

#endif	/* NARRATIVE_RENDER_CAIRO_H */
