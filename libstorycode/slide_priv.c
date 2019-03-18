/*
 * slide_priv.c
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

#include "slide.h"
#include "slide_priv.h"


static double lcalc(struct length l, double pd)
{
	if ( l.unit == LENGTH_UNIT ) {
		return l.len;
	} else {
		return l.len * pd;
	}
}


static enum style_element styel_for_slideitem(enum slide_item_type t)
{
	switch ( t ) {

		case SLIDE_ITEM_TEXT :
		return STYEL_SLIDE_TEXT;

		case SLIDE_ITEM_IMAGE :
		return STYEL_SLIDE_IMAGE;

		case SLIDE_ITEM_PRESTITLE :
		return STYEL_SLIDE_PRESTITLE;

		case SLIDE_ITEM_SLIDETITLE :
		return STYEL_SLIDE_SLIDETITLE;

		case SLIDE_ITEM_FOOTER :
		return STYEL_SLIDE_FOOTER;

	}

	fprintf(stderr, "Invalid slide item %i\n", t);
	return STYEL_SLIDE_TEXT;
}


void slide_item_get_geom(struct slide_item *item, Stylesheet *ss,
                         double *x, double *y, double *w, double *h,
                         double slide_w, double slide_h)
{
	struct frame_geom geom;

	if ( (item->type == SLIDE_ITEM_TEXT)
	  || (item->type == SLIDE_ITEM_IMAGE) )
	{
		geom = item->geom;
	} else {
		if ( stylesheet_get_geometry(ss, styel_for_slideitem(item->type), &geom) ) {
			*x = 0.0;  *y = 0.0;
			*w = 0.0;  *h = 0.0;
			return;
		}
	}

	*x = lcalc(geom.x, slide_w);
	*y = lcalc(geom.y, slide_h);
	*w = lcalc(geom.w, slide_w);
	*h = lcalc(geom.h, slide_h);
}
