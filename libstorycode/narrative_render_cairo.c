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


static double wrap_text(struct narrative_item *item, PangoContext *pc,
                        Stylesheet *ss, enum style_element el, double wrap_w,
                        PangoFontDescription *fontdesc, enum alignment align)
{
	int i;
	PangoAlignment palignment;
	double total_h = 0.0;

	if ( item->layouts == NULL ) {
		item->layouts = malloc(item->n_paras*sizeof(PangoLayout *));
		if ( item->layouts == NULL ) return 0.0;
		for ( i=0; i<item->n_paras; i++ ) {
			item->layouts[i] = NULL;
		}
	}

	if ( item->align == ALIGN_INHERIT ) {
		/* Use value from stylesheet */
		palignment = to_pangoalignment(align);
	} else {
		/* Use item-specific value */
		palignment = to_pangoalignment(item->align);
	}

	for ( i=0; i<item->n_paras; i++ ) {

		PangoRectangle rect;

		if ( item->layouts[i] == NULL ) {
			item->layouts[i] = pango_layout_new(pc);
		}
		pango_layout_set_width(item->layouts[i], pango_units_from_double(wrap_w));
		pango_layout_set_text(item->layouts[i], item->paragraphs[i], -1);
		pango_layout_set_alignment(item->layouts[i], palignment);
		pango_layout_set_font_description(item->layouts[i], fontdesc);

		/* FIXME: Handle *bold*, _underline_, /italic/ etc. */
		//pango_layout_set_attributes(item->layouts[i], attrs);
		//pango_attr_list_unref(attrs);

		pango_layout_get_extents(item->layouts[i], NULL, &rect);
		total_h += pango_units_to_double(rect.height);

	}

	return total_h;
}


static void draw_text(struct narrative_item *item, cairo_t *cr)
{
	int i;
	double hpos = 0.0;

	cairo_save(cr);

	for ( i=0; i<item->n_paras; i++ ) {

		PangoRectangle rect;
		double cur_h;

		pango_layout_get_extents(item->layouts[i], NULL, &rect);
		cur_h = rect.height;

		cairo_save(cr);
		cairo_translate(cr, 0.0, hpos);

		//if ( (hpos + cur_h > min_y) && (hpos < max_y) ) {
			cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
			pango_cairo_update_layout(cr, item->layouts[i]);
			pango_cairo_show_layout(cr, item->layouts[i]);
			cairo_fill(cr);
		//} /* else paragraph is not visible */

		hpos += cur_h;
		cairo_restore(cr);

	}

	cairo_restore(cr);
}


int narrative_wrap(Narrative *n, Stylesheet *stylesheet, PangoLanguage *lang,
                   PangoContext *pc, double w)
{
	int i;
	struct length pad[4];
	double pad_l, pad_r, pad_t, pad_b;
	const char *font;
	PangoFontDescription *fontdesc;
	double wrap_w;
	enum alignment align;

	if ( stylesheet_get_padding(stylesheet, STYEL_NARRATIVE, pad) ) return 1;
	pad_l = lcalc(pad[0], w);
	pad_r = lcalc(pad[1], w);
	pad_t = lcalc(pad[2], 1024.0);  /* dummy value, h not allowed in narrative */
	pad_b = lcalc(pad[3], 1024.0);  /* dummy value, h not allowed in narrative */
	wrap_w = w - pad_l - pad_r;

	n->w = w;
	n->total_h = pad_t + pad_b;

	font = stylesheet_get_font(stylesheet, STYEL_NARRATIVE, NULL, &align);
	if ( font == NULL ) return 1;
	fontdesc = pango_font_description_from_string(font);

	for ( i=0; i<n->n_items; i++ ) {

		switch ( n->items[i].type ) {

			case NARRATIVE_ITEM_TEXT :
			n->total_h += wrap_text(&n->items[i], pc, stylesheet,
			                        STYEL_NARRATIVE, wrap_w, fontdesc,
			                        align);
			break;

			case NARRATIVE_ITEM_BP :
			n->total_h += wrap_text(&n->items[i], pc, stylesheet,
			                        STYEL_NARRATIVE, wrap_w, fontdesc,
			                        align);
			break;

			case NARRATIVE_ITEM_SLIDE :
			break;

			default :
			break;

		}
	}

	return 0;
}


double narrative_get_height(Narrative *n)
{
	return n->total_h;
}


int narrative_render_cairo(Narrative *n, cairo_t *cr, Stylesheet *stylesheet,
                           PangoLanguage *lang, PangoContext *pc, double w)
{
	int i, r;
	enum gradient bg;
	double bgcol[4];
	double bgcol2[4];
	cairo_pattern_t *patt = NULL;

	r = stylesheet_get_background(stylesheet, STYEL_SLIDE, &bg, bgcol, bgcol2);
	if ( r ) return 1;

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, n->w, n->total_h);
	switch ( bg ) {

		case GRAD_NONE:
		cairo_set_source_rgb(cr, bgcol[0], bgcol[1], bgcol[2]);
		break;

		case GRAD_VERT:
		patt = cairo_pattern_create_linear(0.0, 0.0, 0.0, n->total_h);
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

	for ( i=0; i<n->n_items; i++ ) {

		switch ( n->items[i].type ) {

			case NARRATIVE_ITEM_TEXT :
			draw_text(&n->items[i], cr);
			break;

			case NARRATIVE_ITEM_BP :
			draw_text(&n->items[i], cr);
			break;

			case NARRATIVE_ITEM_SLIDE :
			break;

			default :
			break;

		}
	}

	return 0;
}
