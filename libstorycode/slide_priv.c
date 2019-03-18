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


void slide_item_get_padding(struct slide_item *item, Stylesheet *ss,
                            double *l, double *r, double *t, double *b,
                            double slide_w, double slide_h)
{
	struct length padding[4];
	double frx, fry, frw, frh;

	if ( stylesheet_get_padding(ss, styel_for_slideitem(item->type), padding) ) {
		*l = 0.0;  *r = 0.0;  *t = 0.0;  *b = 0.0;
		return;
	}

	slide_item_get_geom(item, ss, &frx, &fry, &frw, &frh, slide_w, slide_h);

	*l = lcalc(padding[0], frw);
	*r = lcalc(padding[1], frh);
	*t = lcalc(padding[2], frw);
	*b = lcalc(padding[3], frh);
}
