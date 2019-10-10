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

#include <libintl.h>
#define _(x) gettext(x)

#include "slide.h"
#include "narrative.h"
#include "stylesheet.h"
#include "imagestore.h"
#include "slide_render_cairo.h"
#include "render_cairo_common.h"

#include "slide_priv.h"


static double slide_lcalc(struct length l, double pd)
{
	if ( l.unit == LENGTH_UNIT ) {
		return l.len;
	} else {
		return l.len * pd;
	}
}


static int slide_positions_equal(struct slide_pos a, struct slide_pos b)
{
	if ( a.para != b.para ) return 0;
	if ( a.pos != b.pos ) return 0;
	if ( a.trail != b.trail ) return 0;
	return 1;
}


static int slide_which_run(struct slide_text_paragraph *para, size_t item_offs,
                           size_t *run_offs)
{
	int run;
	size_t pos = 0;

	assert(para->n_runs > 0);

	for ( run=0; run<para->n_runs; run++ ) {
		size_t npos = pos + strlen(para->runs[run].text);
		if ( npos >= item_offs ) break;
		pos = npos;
	}
	if ( run_offs != NULL ) {
		*run_offs = item_offs - pos;
	}
	return run;
}


static int slide_item_is_text(SlideItem *item)
{
	switch ( item->type ) {

		case SLIDE_ITEM_TEXT : return 1;
		case SLIDE_ITEM_IMAGE : return 0;
		case SLIDE_ITEM_FOOTER : return 0;
		case SLIDE_ITEM_SLIDETITLE : return 1;
		case SLIDE_ITEM_PRESTITLE : return 1;

		default :
		fprintf(stderr, "Please update slide_item_is_text\n");
		return 0;
	}
}


size_t slide_pos_trail_to_offset(SlideItem *item, int para_num, size_t offs, int trail)
{
	glong char_offs;
	char *ptr;
	int run;
	size_t run_offs;
	size_t prev_len;
	int j;
	struct slide_text_paragraph *para;

	if ( !slide_item_is_text(item) ) return offs;

	para = &item->paras[para_num];
	run = slide_which_run(para, offs, &run_offs);

	char_offs = g_utf8_pointer_to_offset(para->runs[run].text,
	                                     para->runs[run].text+run_offs);
	char_offs += trail;
	ptr = g_utf8_offset_to_pointer(para->runs[run].text, char_offs);

	prev_len = 0;
	for ( j=0; j<run; j++ ) prev_len += strlen(para->runs[j].text);
	return prev_len + ptr - para->runs[run].text;
}


static void do_background(Stylesheet *ss, const char *stn, cairo_t *cr,
                          double x, double y, double w, double h)
{
	enum gradient bg;
	struct colour bgcol;
	struct colour bgcol2;
	cairo_pattern_t *patt = NULL;

	if ( stylesheet_get_background(ss, stn, &bg, &bgcol, &bgcol2) ) return;

	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_rectangle(cr, 0.0, 0.0, w, h);

	switch ( bg ) {

		case GRAD_NONE:
		cairo_set_source_rgba(cr, bgcol.rgba[0], bgcol.rgba[1],
		                           bgcol.rgba[2], bgcol.rgba[3]);
		break;

		case GRAD_VERT:
		patt = cairo_pattern_create_linear(0.0, 0.0, 0.0, h);
		cairo_pattern_add_color_stop_rgba(patt, 0.0,
		                                  bgcol.rgba[0], bgcol.rgba[1],
		                                  bgcol.rgba[2], bgcol.rgba[3]);
		cairo_pattern_add_color_stop_rgba(patt, 1.0,
		                                  bgcol2.rgba[0], bgcol2.rgba[1],
		                                  bgcol2.rgba[2], bgcol2.rgba[3]);
		cairo_set_source(cr, patt);
		break;

		case GRAD_HORIZ:
		patt = cairo_pattern_create_linear(0.0, 0.0, w, 0.0);
		cairo_pattern_add_color_stop_rgba(patt, 0.0,
		                                  bgcol.rgba[0], bgcol.rgba[1],
		                                  bgcol.rgba[2], bgcol.rgba[3]);
		cairo_pattern_add_color_stop_rgba(patt, 1.0,
		                                  bgcol2.rgba[0], bgcol2.rgba[1],
		                                  bgcol2.rgba[2], bgcol2.rgba[3]);
		cairo_set_source(cr, patt);
		break;

		default:
		fprintf(stderr, "Unrecognised slide background type %i\n", bg);
		break;

	}
	cairo_fill(cr);
	cairo_restore(cr);
}


static void render_text(SlideItem *item, cairo_t *cr, PangoContext *pc,
                        Stylesheet *ss, const char *stn,
                        double parent_w, double parent_h,
                        struct slide_pos sel_start, struct slide_pos sel_end)
{
	int i;
	double x, y, w, h;
	double pad_l, pad_r, pad_t;
	struct length pad[4];
	struct length paraspacel[4];
	double paraspace[4];
	const char *font;
	enum alignment align;
	struct colour fgcol;
	PangoRectangle rect;
	PangoFontDescription *fontdesc;
	PangoAlignment palignment;
	size_t sel_s, sel_e;
	struct frame_geom geom;

	if ( (item->type == SLIDE_ITEM_TEXT) || (item->type == SLIDE_ITEM_IMAGE) ) {
		geom = item->geom;
	} else {
		stylesheet_get_geometry(ss, stn, &geom);
	}
	x = slide_lcalc(geom.x, parent_w);
	y = slide_lcalc(geom.y, parent_h);
	w = slide_lcalc(geom.w, parent_w);
	h = slide_lcalc(geom.h, parent_h);

	if ( stylesheet_get_padding(ss, stn, pad) ) return;
	pad_l = slide_lcalc(pad[0], parent_w);
	pad_r = slide_lcalc(pad[1], parent_w);
	pad_t = slide_lcalc(pad[2], parent_h);

	font = stylesheet_get_font(ss, stn, &fgcol, &align);
	if ( font == NULL ) return;

	fontdesc = pango_font_description_from_string(font);

	if ( item->align == ALIGN_INHERIT ) {
		/* Use value from stylesheet */
		palignment = to_pangoalignment(align);
	} else {
		/* Use item-specific value */
		palignment = to_pangoalignment(item->align);
	}

	if ( !slide_positions_equal(sel_start, sel_end) ) {
		sel_s = slide_pos_trail_to_offset(item, sel_start.para,
		                                  sel_start.pos, sel_start.trail);
		sel_e = slide_pos_trail_to_offset(item, sel_end.para,
		                                  sel_end.pos, sel_end.trail);
	} else {
		sel_s = 0;
		sel_e = 0;
	}

	do_background(ss, stn, cr, x, y, w, h);

	if ( stylesheet_get_paraspace(ss, stn, paraspacel) == 0 ) {
		paraspace[0] = slide_lcalc(paraspacel[0], w);
		paraspace[1] = slide_lcalc(paraspacel[1], w);
		paraspace[2] = slide_lcalc(paraspacel[2], h);
		paraspace[3] = slide_lcalc(paraspacel[3], h);
	} else {
		paraspace[0] = 0.0;
		paraspace[1] = 0.0;
		paraspace[2] = 0.0;
		paraspace[3] = 0.0;
	}

	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_translate(cr, pad_l+paraspace[0], pad_t);

	for ( i=0; i<item->n_paras; i++ ) {

		PangoAttrList *attrs;

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

		if ( item->paras[i].layout == NULL ) {
			item->paras[i].layout = pango_layout_new(pc);
		}
		pango_layout_set_width(item->paras[i].layout,
		                       pango_units_from_double(w-pad_l-pad_r));
		pango_layout_set_alignment(item->paras[i].layout, palignment);
		pango_layout_set_font_description(item->paras[i].layout, fontdesc);

		attrs = pango_attr_list_new();

		if ( srt > 0 || end > 0 ) {
			PangoAttribute *attr;
			attr = pango_attr_background_new(42919, 58853, 65535);
			attr->start_index = srt;
			attr->end_index = end;
			pango_attr_list_insert(attrs, attr);
		}

		pango_layout_set_attributes(item->paras[i].layout, attrs);

		runs_to_pangolayout(item->paras[i].layout, item->paras[i].runs,
		                    item->paras[i].n_runs);

		pango_attr_list_unref(attrs);

		/* FIXME: Clip to w,h */

		cairo_set_source_rgba(cr, fgcol.rgba[0], fgcol.rgba[1], fgcol.rgba[2],
		                          fgcol.rgba[3]);
		cairo_translate(cr, 0.0, paraspace[2]);
		cairo_move_to(cr, 0.0, 0.0);
		pango_cairo_update_layout(cr, item->paras[i].layout);
		pango_cairo_show_layout(cr, item->paras[i].layout);
		pango_layout_get_extents(item->paras[i].layout, NULL, &rect);
		cairo_translate(cr, 0.0, pango_units_to_double(rect.height)+paraspace[3]);
		cairo_fill(cr);

	}
	cairo_restore(cr);
}


static void render_image(SlideItem *item, cairo_t *cr,
                         Stylesheet *ss, ImageStore *is,
                         double parent_w, double parent_h)
{
	double x, y, w, h;
	double wd, hd;
	cairo_surface_t *surf;

	x = slide_lcalc(item->geom.x, parent_w);
	y = slide_lcalc(item->geom.y, parent_h);
	w = slide_lcalc(item->geom.w, parent_w);
	h = slide_lcalc(item->geom.h, parent_h);

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


static void sort_slide_positions(struct slide_pos *a, struct slide_pos *b)
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


int slide_render_cairo(Slide *s, cairo_t *cr, ImageStore *is, Stylesheet *stylesheet,
                       int slide_number, PangoLanguage *lang, PangoContext *pc,
                       SlideItem *sel_item, struct slide_pos sel_start, struct slide_pos sel_end)
{
	int i;
	double w, h;

	slide_get_logical_size(s, stylesheet, &w, &h);
	sort_slide_positions(&sel_start, &sel_end);

	/* Overall background */
	do_background(stylesheet, "SLIDE", cr, 0.0, 0.0, w, h);

	for ( i=0; i<s->n_items; i++ ) {

		struct slide_pos srt, end;

		if ( &s->items[i] != sel_item ) {
			srt.para = 0;  srt.pos = 0;  srt.trail = 0;
			end.para = 0;  end.pos = 0;  end.trail = 0;
		} else {
			srt = sel_start;  end = sel_end;
		}

		switch ( s->items[i].type ) {

			case SLIDE_ITEM_TEXT :
			render_text(&s->items[i], cr, pc, stylesheet, "SLIDE.TEXT",
			            w, h, srt, end);
			break;

			case SLIDE_ITEM_IMAGE :
			render_image(&s->items[i], cr, stylesheet, is,
			             w, h);
			break;

			case SLIDE_ITEM_SLIDETITLE :
			render_text(&s->items[i], cr, pc, stylesheet, "SLIDE.SLIDETITLE",
			            w, h, srt, end);
			break;

			case SLIDE_ITEM_PRESTITLE :
			render_text(&s->items[i], cr, pc, stylesheet, "SLIDE.PRESTITLE",
			            w, h, srt, end);
			break;

			default :
			break;

		}
	}

	return 0;
}


int render_slides_to_pdf(Narrative *n, ImageStore *is, const char *filename)
{
	double w = 2048.0;
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;
	PangoContext *pc;
	struct slide_pos sel;

	surf = cairo_pdf_surface_create(filename, w, w);
	if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
		fprintf(stderr, _("Couldn't create Cairo surface\n"));
		return 1;
	}

	cr = cairo_create(surf);
	pc = pango_cairo_create_context(cr);

	sel.para = 0;  sel.pos = 0;  sel.trail = 0;

	for ( i=0; i<narrative_get_num_slides(n); i++ )
	{
		Slide *s;
		double log_w, log_h;

		s = narrative_get_slide_by_number(n, i);
		slide_get_logical_size(s, narrative_get_stylesheet(n),
		                       &log_w, &log_h);

		cairo_pdf_surface_set_size(surf, w, w*(log_h/log_w));

		cairo_save(cr);
		cairo_scale(cr, w/log_w, w/log_w);
		slide_render_cairo(s, cr, is, narrative_get_stylesheet(n),
		                   i, pango_language_get_default(), pc,
		                   NULL, sel, sel);
		cairo_show_page(cr);
		cairo_restore(cr);
	}

	g_object_unref(pc);
	cairo_surface_finish(surf);
	cairo_destroy(cr);

	return 0;
}
