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
#include "imagestore.h"
#include "slide_render_cairo.h"
#include "narrative_render_cairo.h"

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
                      Stylesheet *ss, enum style_element el, double w,
                      size_t sel_start, size_t sel_end)
{
	PangoAlignment palignment;
	PangoRectangle rect;
	const char *font;
	PangoFontDescription *fontdesc;
	enum alignment align;
	struct length paraspace[4];
	double wrap_w;
	PangoAttrList *attrs;
	PangoAttribute *attr;
	struct colour fgcol;
	guint16 r, g, b;

	font = stylesheet_get_font(ss, el, &fgcol, &align);
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

	/* Set foreground colour */
	attrs = pango_attr_list_new();
	r = fgcol.rgba[0] * 65535;
	g = fgcol.rgba[1] * 65535;
	b = fgcol.rgba[2] * 65535;
	attr = pango_attr_foreground_new(r, g, b);
	pango_attr_list_insert(attrs, attr);

	/* Add attributes for selected text */
	if ( sel_start > 0 || sel_end > 0 ) {
		PangoAttribute *attr;
		attr = pango_attr_background_new(42919, 58853, 65535);
		attr->start_index = sel_start;
		attr->end_index = sel_end;
		pango_attr_list_insert(attrs, attr);
	}

	if ( item->layout == NULL ) {
		item->layout = pango_layout_new(pc);
	}
	pango_layout_set_width(item->layout, pango_units_from_double(wrap_w));
	pango_layout_set_text(item->layout, item->text, -1);
	pango_layout_set_alignment(item->layout, palignment);
	pango_layout_set_font_description(item->layout, fontdesc);
	pango_layout_set_attributes(item->layout, attrs);

	/* FIXME: Handle *bold*, _underline_, /italic/ etc. */
	//pango_layout_set_attributes(item->layout, attrs);
	//pango_attr_list_unref(attrs);

	pango_layout_get_extents(item->layout, NULL, &rect);
	item->h = pango_units_to_double(rect.height)+item->space_t+item->space_b;
}


/* Render a thumbnail of the slide */
static cairo_surface_t *render_thumbnail(Slide *s, Stylesheet *ss, ImageStore *is,
                                         int w, int h)
{
	cairo_surface_t *surf;
	cairo_surface_t *full_surf;
	cairo_t *cr;
	double logical_w, logical_h;
	PangoContext *pc;
	const int rh = 1024; /* "reasonably big" height for slide */
	int rw;
	struct slide_pos sel;

	slide_get_logical_size(s, ss, &logical_w, &logical_h);
	rw = rh*(logical_w/logical_h);

	sel.para = 0;  sel.pos = 0;  sel.trail = 0;

	/* Render at a reasonably big size.  Rendering to a small surface makes
	 * rounding of text positions (due to font hinting) cause significant
	 * differences between the thumbnail and "normal" rendering. */
	full_surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, rw, rh);
	cr = cairo_create(full_surf);
	cairo_scale(cr, (double)rw/logical_w, (double)rh/logical_h);
	pc = pango_cairo_create_context(cr);
	slide_render_cairo(s, cr, is, ss, 0, pango_language_get_default(), pc,
	                   NULL, sel, sel);
	g_object_unref(pc);
	cairo_destroy(cr);

	/* Scale down to the actual size of the thumbnail */
	surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
	cr = cairo_create(surf);
	cairo_scale(cr, (double)w/rw, (double)h/rh);
	cairo_set_source_surface(cr, full_surf, 0.0, 0.0);
	cairo_paint(cr);
	cairo_destroy(cr);

	cairo_surface_destroy(full_surf);
	return surf;
}


static void wrap_slide(struct narrative_item *item, Stylesheet *ss, ImageStore *is)
{
	double w, h;

	item->space_l = 0.0;
	item->space_r = 0.0;
	item->space_t = 10.0;
	item->space_b = 10.0;

	slide_get_logical_size(item->slide, ss, &w, &h);
	item->slide_h = 320.0;  /* Actual height of thumbnail */
	item->slide_w = rint(item->slide_h*w/h);

	item->h = item->slide_h + item->space_t + item->space_b;

	if ( item->slide_thumbnail != NULL ) {
		cairo_surface_destroy(item->slide_thumbnail);
	}
	item->slide_thumbnail = render_thumbnail(item->slide, ss, is,
	                                         item->slide_w, item->slide_h);
}


static size_t pos_trail_to_offset(struct narrative_item *item, int offs, int trail)
{
	glong char_offs;
	char *ptr;

	if ( item->type == NARRATIVE_ITEM_SLIDE ) {
		return offs;
	}

	char_offs = g_utf8_pointer_to_offset(item->text, item->text+offs);
	char_offs += trail;
	ptr = g_utf8_offset_to_pointer(item->text, char_offs);
	return ptr - item->text;
}


static int positions_equal(struct edit_pos a, struct edit_pos b)
{
	if ( a.para != b.para ) return 0;
	if ( a.pos != b.pos ) return 0;
	if ( a.trail != b.trail ) return 0;
	return 1;
}


static void sort_positions(struct edit_pos *a, struct edit_pos *b)
{
	if ( a->para > b->para ) {
		size_t tpos;
		int tpara, ttrail;
		tpara = b->para;   tpos = b->pos;  ttrail = b->trail;
		b->para = a->para;  b->pos = a->pos;  b->trail = a->trail;
		a->para = tpara;    a->pos = tpos;    a->trail = ttrail;
	}

	if ( (a->para == b->para) && (a->pos > b->pos) )
	{
		size_t tpos = b->pos;
		int ttrail = b->trail;
		b->pos = a->pos;  b->trail = a->trail;
		a->pos = tpos;    a->trail = ttrail;
	}
}


int narrative_wrap_range(Narrative *n, Stylesheet *stylesheet, PangoLanguage *lang,
                         PangoContext *pc, double w, ImageStore *is,
                         int min, int max,
                         struct edit_pos sel_start, struct edit_pos sel_end)
{
	int i;
	struct length pad[4];
	int sel_s, sel_e;

	if ( stylesheet_get_padding(stylesheet, STYEL_NARRATIVE, pad) ) return 1;
	n->space_l = lcalc(pad[0], w);
	n->space_r = lcalc(pad[1], w);
	n->space_t = lcalc(pad[2], dummy_h_val);
	n->space_b = lcalc(pad[3], dummy_h_val);

	n->w = w;
	w -= n->space_l + n->space_r;

	sort_positions(&sel_start, &sel_end);
	if ( min < 0 ) min = 0;
	if ( max >= n->n_items ) max = n->n_items-1;

	if ( !positions_equal(sel_start, sel_end) ) {
		struct narrative_item *item;
		item = &n->items[sel_start.para];
		sel_s = pos_trail_to_offset(item, sel_start.pos, sel_start.trail);
		item = &n->items[sel_end.para];
		sel_e = pos_trail_to_offset(item, sel_end.pos, sel_end.trail);
	} else {
		sel_s = 0;
		sel_e = 0;
	}

	for ( i=min; i<=max; i++ ) {

		size_t srt, end;
		if ( i >= sel_start.para && i <= sel_end.para ) {
			if ( i == sel_start.para ) {
				srt = sel_s;
			} else {
				srt = 0;
			}
			if ( i == sel_end.para ) {
				end = sel_e;
			} else {
				end = G_MAXUINT;
			}
			if ( i > sel_start.para && i < sel_end.para ) {
				end = G_MAXUINT;
			}
		} else {
			srt = 0;
			end = 0;
		}

		switch ( n->items[i].type ) {

			case NARRATIVE_ITEM_TEXT :
			wrap_text(&n->items[i], pc, stylesheet,
			          STYEL_NARRATIVE, w, srt, end);
			break;

			case NARRATIVE_ITEM_BP :
			wrap_text(&n->items[i], pc, stylesheet,
			          STYEL_NARRATIVE_BP, w, srt, end);
			break;

			case NARRATIVE_ITEM_PRESTITLE :
			wrap_text(&n->items[i], pc, stylesheet,
			          STYEL_NARRATIVE_PRESTITLE, w, srt, end);
			break;

			case NARRATIVE_ITEM_SLIDE :
			wrap_slide(&n->items[i], stylesheet, is);
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

	cairo_rectangle(cr, x, y, item->slide_w, item->slide_h);
	cairo_set_source_surface(cr, item->slide_thumbnail, 0.0, 0.0);
	cairo_fill(cr);

	cairo_rectangle(cr, x+0.5, y+0.5, item->slide_w, item->slide_h);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	cairo_restore(cr);
}


static void draw_text(struct narrative_item *item, cairo_t *cr)
{
	if ( item->layout == NULL ) return;

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
	struct colour bgcol;
	struct colour bgcol2;
	cairo_pattern_t *patt = NULL;

	r = stylesheet_get_background(stylesheet, STYEL_NARRATIVE, &bg, &bgcol, &bgcol2);
	if ( r ) return 1;

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, n->w, narrative_get_height(n));
	switch ( bg ) {

		case GRAD_NONE:
		cairo_set_source_rgb(cr, bgcol.rgba[0], bgcol.rgba[1], bgcol.rgba[2]);
		break;

		case GRAD_VERT:
		patt = cairo_pattern_create_linear(0.0, 0.0, 0.0, narrative_get_height(n));
		cairo_pattern_add_color_stop_rgb(patt, 0.0,
		                                 bgcol.rgba[0], bgcol.rgba[1], bgcol.rgba[2]);
		cairo_pattern_add_color_stop_rgb(patt, 1.0,
		                                 bgcol2.rgba[0], bgcol2.rgba[1], bgcol2.rgba[2]);
		cairo_set_source(cr, patt);
		break;

		case GRAD_HORIZ:
		patt = cairo_pattern_create_linear(0.0, 0.0, n->w, 0.0);
		cairo_pattern_add_color_stop_rgb(patt, 0.0,
		                                 bgcol.rgba[0], bgcol.rgba[1], bgcol.rgba[2]);
		cairo_pattern_add_color_stop_rgb(patt, 1.0,
		                                 bgcol2.rgba[0], bgcol2.rgba[1], bgcol2.rgba[2]);
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
