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
#include <math.h>

#include "presentation.h"
#include "slide.h"
#include "narrative.h"
#include "stylesheet.h"
#include "imagestore.h"

#include "slide_priv.h"


static double lcalc(struct length l, double pd)
{
	if ( l.unit == LENGTH_UNIT ) {
		return l.len;
	} else {
		return l.len * pd;
	}
}


static PangoAlignment to_pangoalignment(enum alignment align)
{
	switch ( align ) {
		case ALIGN_LEFT : return PANGO_ALIGN_LEFT;
		case ALIGN_RIGHT : return PANGO_ALIGN_RIGHT;
		case ALIGN_CENTER : return PANGO_ALIGN_CENTER;
		default: return PANGO_ALIGN_LEFT;
	}
}


static void render_text(struct slide_item *item, cairo_t *cr, PangoContext *pc,
                        Stylesheet *ss, double parent_w, double parent_h)
{
	int i;
	double x, y, w, h;
	PangoRectangle rect;
	PangoFontDescription *fontdesc;

	x = lcalc(item->geom.x, parent_w);
	y = lcalc(item->geom.y, parent_h);
	w = lcalc(item->geom.w, parent_w);
	h = lcalc(item->geom.h, parent_h);

	fontdesc = pango_font_description_from_string(stylesheet_get_slide_text_font(ss));

	if ( item->layouts == NULL ) {
		item->layouts = malloc(item->n_paras*sizeof(PangoLayout *));
		if ( item->layouts == NULL ) return;
		for ( i=0; i<item->n_paras; i++ ) {
			item->layouts[i] = NULL;
		}
	}

	for ( i=0; i<item->n_paras; i++ ) {

		if ( item->layouts[i] == NULL ) {
			item->layouts[i] = pango_layout_new(pc);
		}
		pango_layout_set_width(item->layouts[i],
		                       pango_units_from_double(w));
		pango_layout_set_text(item->layouts[i], item->paragraphs[i], -1);

		pango_layout_set_alignment(item->layouts[i],
		                           to_pangoalignment(item->align));

		pango_layout_set_font_description(item->layouts[i], fontdesc);

		/* FIXME: Handle *bold*, _underline_, /italic/ etc. */
		//pango_layout_set_attributes(item->layouts[i], attrs);
		//pango_attr_list_unref(attrs);

		/* FIXME: Clip to w,h */

		cairo_save(cr);
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
		cairo_translate(cr, x, y);
		pango_cairo_update_layout(cr, item->layouts[i]);
		pango_cairo_show_layout(cr, item->layouts[i]);
		pango_layout_get_extents(item->layouts[i], NULL, &rect);
		y += pango_units_to_double(rect.height);
		cairo_fill(cr);
		cairo_restore(cr);

	}
}


static void render_image(struct slide_item *item, cairo_t *cr,
                         Stylesheet *ss, ImageStore *is,
                         double parent_w, double parent_h)
{
	double x, y, w, h;
	double wd, hd;
	cairo_surface_t *surf;

	x = lcalc(item->geom.x, parent_w);
	y = lcalc(item->geom.y, parent_h);
	w = lcalc(item->geom.w, parent_w);
	h = lcalc(item->geom.h, parent_h);

	wd = w;  hd = h;
	cairo_user_to_device_distance(cr, &wd, &hd);
	surf = lookup_image(is, item->filename, wd);
	if ( surf == NULL ) return;

	cairo_user_to_device(cr, &x, &y);
	x = rint(x);  y = rint(y);
	cairo_device_to_user(cr, &x, &y);

	cairo_save(cr);
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, w, h);
	cairo_translate(cr, x, y);
	cairo_scale(cr, w/wd, h/hd);
	cairo_set_source_surface(cr, surf, 0.0, 0.0);
	cairo_pattern_t *patt = cairo_get_source(cr);
	cairo_pattern_set_extend(patt, CAIRO_EXTEND_NONE);
	cairo_pattern_set_filter(patt, CAIRO_FILTER_BEST);
	cairo_fill(cr);
	cairo_restore(cr);
}


int slide_render_cairo(Slide *s, cairo_t *cr, ImageStore *is, Stylesheet *stylesheet,
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
			render_text(&s->items[i], cr, pc, stylesheet,
			            s->logical_w, s->logical_h);
			break;

			case SLIDE_ITEM_IMAGE :
			render_image(&s->items[i], cr, stylesheet, is,
			             s->logical_w, s->logical_h);
			break;

			default :
			break;

		}
	}

	return 0;
}
