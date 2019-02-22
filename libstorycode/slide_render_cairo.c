/*
 * render.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "presentation.h"
#include "slide.h"
#include "narrative.h"
#include "stylesheet.h"

#include "slide_priv.h"


static double lcalc(struct length l, double pd)
{
	if ( l.unit == LENGTH_UNIT ) {
		return l.len;
	} else {
		return l.len * pd;
	}
}


static void render_text(struct slide_item *item, cairo_t *cr,
                        double parent_w, double parent_h)
{
	cairo_rectangle(cr, lcalc(item->geom.x, parent_w),
	                    lcalc(item->geom.y, parent_h),
	                    lcalc(item->geom.w, parent_w),
	                    lcalc(item->geom.h, parent_h));
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
}


static void render_image(struct slide_item *item, cairo_t *cr,
                         double parent_w, double parent_h)
{
	cairo_rectangle(cr, lcalc(item->geom.x, parent_w),
	                    lcalc(item->geom.y, parent_h),
	                    lcalc(item->geom.w, parent_w),
	                    lcalc(item->geom.h, parent_h));
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgba(cr, 0.0, 0.5, 0.0, 0.5);
	cairo_fill(cr);
}


int slide_render_cairo(Slide *s, cairo_t *cr, Stylesheet *stylesheet,
                       int slide_number, PangoLanguage *lang, PangoContext *pc)
{
	int i;

	/* Overall default background */
	cairo_rectangle(cr, 0.0, 0.0, s->logical_w, s->logical_h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	for ( i=0; i<s->n_items; i++ ) {

		switch ( s->items[i].type ) {

			case SLIDE_ITEM_TEXT :
			render_text(&s->items[i], cr, s->logical_w, s->logical_h);
			break;

			case SLIDE_ITEM_IMAGE :
			render_image(&s->items[i], cr, s->logical_w, s->logical_h);
			break;

			default :
			break;

		}
	}

	return 0;
}
