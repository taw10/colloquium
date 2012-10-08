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
#include <string.h>

#include "presentation.h"
#include "layout.h"
#include "stylesheet.h"


static void copy_lop_from_style(struct frame *fr, struct style *style)
{
	if ( style == NULL ) return;  /* Not dictated by style sheet */

	memcpy(&fr->lop, &style->lop, sizeof(struct layout_parameters));
}


static void layout_subframe(struct frame *fr, double w, double h,
                            PangoContext *pc)
{
	int i;

	fr->w = w;
	fr->h = h;

	for ( i=0; i<fr->num_ro; i++ ) {

		struct frame *child;
		double child_w, child_h;
		double offs_x, offs_y;
		PangoFontDescription *fontdesc;

		child = fr->rendering_order[i];

		if ( child == fr ) continue;

		copy_lop_from_style(child, child->style);

		/* First, calculate the region inside which the child frame must
		 * fit, having excluded the padding of its parent and its own
		 * margins. */
		offs_x = child->lop.margin_l + fr->lop.pad_l;
		offs_y = child->lop.margin_t + fr->lop.pad_r;
		child_w = w - (child->lop.margin_l + child->lop.margin_r);
		child_h = h - (child->lop.margin_t + child->lop.margin_b);
		child_w -= (fr->lop.pad_l + fr->lop.pad_r);
		child_h -= (fr->lop.pad_t + fr->lop.pad_b);

		/* Put the contents inside, and see how much space is needed */
		if ( child->pl == NULL ) {
			child->pl = pango_layout_new(pc);
			pango_layout_set_justify(child->pl, 1);
			pango_layout_set_ellipsize(child->pl, 1);
		}
		pango_layout_set_width(child->pl, child_w*PANGO_SCALE);
		pango_layout_set_height(child->pl, child_h*PANGO_SCALE);

		/* FIXME: Check for Level 1 markup e.g. image */
		pango_layout_set_text(child->pl, child->sc, -1);

		fontdesc = pango_font_description_from_string("Sans 12");
		pango_layout_set_font_description(child->pl, fontdesc);
		pango_font_description_free(fontdesc);

		/* Now, apply the minimum size if given */
		if ( child->lop.min_w > 0.0 ) {
			if ( child_w < child->lop.min_w ) {
				child_w = child->lop.min_w;
			}
		}
		if ( child->lop.min_h > 0.0 ) {
			if ( child_h < child->lop.min_h ) {
				child_h = child->lop.min_h;
			}
		}

		/* Finally, apply the gravity */
		switch ( child->lop.grav )
		{
			case DIR_NONE :
			break;

			case DIR_UL :
			case DIR_U :
			case DIR_UR :
			case DIR_R :
			case DIR_DR :
			case DIR_D :
			case DIR_DL :
			case DIR_L :
			break;
		}

		/* Record values and recurse */
		child->offs_x = offs_x;
		child->offs_y = offs_y;
		layout_subframe(child, child_w, child_h, pc);

	}
}


void layout_frame(struct frame *fr, double w, double h, PangoContext *pc)
{
	copy_lop_from_style(fr, fr->style);
	layout_subframe(fr, w, h, pc);
}
