/*
 * narrative_render_cairo.c
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

#include "slide.h"
#include "narrative.h"
#include "stylesheet.h"

#include "narrative_priv.h"

const double dummy_h_val = 1024.0;


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


static void wrap_text(struct narrative_item *item, PangoContext *pc,
                      Stylesheet *ss, enum style_element el, double w)
{
	PangoAlignment palignment;
	PangoRectangle rect;
	const char *font;
	PangoFontDescription *fontdesc;
	enum alignment align;
	struct length paraspace[4];
	double wrap_w;

	font = stylesheet_get_font(ss, el, NULL, &align);
	if ( font == NULL ) return;
	fontdesc = pango_font_description_from_string(font);

	if ( item->align == ALIGN_INHERIT ) {
		/* Use value from stylesheet */
		palignment = to_pangoalignment(align);
	} else {
		/* Use item-specific value */
		palignment = to_pangoalignment(item->align);
	}

	if ( stylesheet_get_paraspace(ss, el, paraspace) ) return;
	item->space_l = lcalc(paraspace[0], w);
	item->space_r = lcalc(paraspace[1], w);
	item->space_t = lcalc(paraspace[2], dummy_h_val);
	item->space_b = lcalc(paraspace[3], dummy_h_val);

	/* Calculate width of actual text */
	wrap_w = w - item->space_l - item->space_r;

	if ( item->layout == NULL ) {
		item->layout = pango_layout_new(pc);
	}
	pango_layout_set_width(item->layout, pango_units_from_double(wrap_w));
	pango_layout_set_text(item->layout, item->text, -1);
	pango_layout_set_alignment(item->layout, palignment);
	pango_layout_set_font_description(item->layout, fontdesc);

	/* FIXME: Handle *bold*, _underline_, /italic/ etc. */
	//pango_layout_set_attributes(item->layout, attrs);
	//pango_attr_list_unref(attrs);


	pango_layout_get_extents(item->layout, NULL, &rect);
	item->h = pango_units_to_double(rect.height)+item->space_t+item->space_b;
}


static void wrap_slide(struct narrative_item *item, Stylesheet *ss)
{
	double w, h;

	item->space_l = 0.0;
	item->space_r = 0.0;
	item->space_t = 10.0;
	item->space_b = 10.0;

	slide_get_logical_size(item->slide, ss, &w, &h);
	item->slide_h = 320.0;
	item->slide_w = item->slide_h*w/h;

	item->h = item->slide_h + item->space_t + item->space_b;

}


int narrative_wrap(Narrative *n, Stylesheet *stylesheet, PangoLanguage *lang,
                   PangoContext *pc, double w)
{
	int i;
	struct length pad[4];

	if ( stylesheet_get_padding(stylesheet, STYEL_NARRATIVE, pad) ) return 1;
	n->space_l = lcalc(pad[0], w);
	n->space_r = lcalc(pad[1], w);
	n->space_t = lcalc(pad[2], dummy_h_val);
	n->space_b = lcalc(pad[3], dummy_h_val);

	n->w = w;
	w -= n->space_l + n->space_r;

	for ( i=0; i<n->n_items; i++ ) {

		switch ( n->items[i].type ) {

			case NARRATIVE_ITEM_TEXT :
			wrap_text(&n->items[i], pc, stylesheet,
			          STYEL_NARRATIVE, w);
			break;

			case NARRATIVE_ITEM_BP :
			wrap_text(&n->items[i], pc, stylesheet,
			          STYEL_NARRATIVE_BP, w);
			break;

			case NARRATIVE_ITEM_PRESTITLE :
			wrap_text(&n->items[i], pc, stylesheet,
			          STYEL_NARRATIVE_PRESTITLE, w);
			break;

			case NARRATIVE_ITEM_SLIDE :
			wrap_slide(&n->items[i], stylesheet);
			break;

			default :
			break;

		}
	}

	return 0;
}


double narrative_get_height(Narrative *n)
{
	int i;
	double total = 0.0;
	for ( i=0; i<n->n_items; i++ ) {
		total += n->items[i].h;
	}
	return total + n->space_t + n->space_b;
}


static void draw_slide(struct narrative_item *item, cairo_t *cr)
{
	double x, y;

	cairo_save(cr);
	cairo_translate(cr, item->space_l, item->space_t);

	x = 0.0;  y = 0.0;
	cairo_user_to_device(cr, &x, &y);
	x = rint(x);  y = rint(y);
	cairo_device_to_user(cr, &x, &y);
	cairo_rectangle(cr, x+0.5, y+0.5, item->slide_w, item->slide_h);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	cairo_restore(cr);
}


static void draw_text(struct narrative_item *item, cairo_t *cr)
{
	cairo_save(cr);
	cairo_translate(cr, item->space_l, item->space_t);

	//if ( (hpos + cur_h > min_y) && (hpos < max_y) ) {
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
		pango_cairo_update_layout(cr, item->layout);
		pango_cairo_show_layout(cr, item->layout);
		cairo_fill(cr);
	//} /* else paragraph is not visible */

	cairo_restore(cr);
}


/* NB You must first call narrative_wrap() */
int narrative_render_cairo(Narrative *n, cairo_t *cr, Stylesheet *stylesheet)
{
	int i, r;
	enum gradient bg;
	double bgcol[4];
	double bgcol2[4];
	cairo_pattern_t *patt = NULL;

	r = stylesheet_get_background(stylesheet, STYEL_NARRATIVE, &bg, bgcol, bgcol2);
	if ( r ) return 1;

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, n->w, narrative_get_height(n));
	switch ( bg ) {

		case GRAD_NONE:
		cairo_set_source_rgb(cr, bgcol[0], bgcol[1], bgcol[2]);
		break;

		case GRAD_VERT:
		patt = cairo_pattern_create_linear(0.0, 0.0, 0.0, narrative_get_height(n));
		cairo_pattern_add_color_stop_rgb(patt, 0.0, bgcol[0], bgcol[1], bgcol[2]);
		cairo_pattern_add_color_stop_rgb(patt, 1.0, bgcol2[0], bgcol2[1], bgcol2[2]);
		cairo_set_source(cr, patt);
		break;

		case GRAD_HORIZ:
		patt = cairo_pattern_create_linear(0.0, 0.0, n->w, 0.0);
		cairo_pattern_add_color_stop_rgb(patt, 0.0, bgcol[0], bgcol[1], bgcol[2]);
		cairo_pattern_add_color_stop_rgb(patt, 1.0, bgcol2[0], bgcol2[1], bgcol2[2]);
		cairo_set_source(cr, patt);
		break;

	}
	cairo_fill(cr);

	cairo_save(cr);
	cairo_translate(cr, n->space_l, n->space_t);

	for ( i=0; i<n->n_items; i++ ) {

		switch ( n->items[i].type ) {

			case NARRATIVE_ITEM_TEXT :
			case NARRATIVE_ITEM_PRESTITLE :
			draw_text(&n->items[i], cr);
			break;

			case NARRATIVE_ITEM_BP :
			draw_text(&n->items[i], cr);
			break;

			case NARRATIVE_ITEM_SLIDE :
			draw_slide(&n->items[i], cr);
			break;

			default :
			break;

		}

		cairo_translate(cr, 0.0, n->items[i].h);
	}

	cairo_restore(cr);

	return 0;
}
