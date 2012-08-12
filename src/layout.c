/*
 * layout.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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

#include "presentation.h"
#include "layout.h"


/* Calculate layout for frame (and all its children) based on size */
void layout_frame(struct frame *fr, double w, double h)
{
	fr->w = w;
	fr->h = h;

	int i;

	for ( i=0; i<fr->num_ro; i++ ) {

		struct frame *child;
		double child_w, child_h;

		child = fr->rendering_order[i];

		if ( child == fr ) continue;

		child->x = fr->lop.margin_l;
		child->y = fr->lop.margin_t;
		child_w = w - (fr->lop.margin_l + fr->lop.margin_r);
		child_h = h - (fr->lop.margin_t + fr->lop.margin_b);
		layout_frame(child, child_w, child_h);

	}
}
