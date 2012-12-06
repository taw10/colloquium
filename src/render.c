/*
 * render.c
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

#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <assert.h>
#include <gdk/gdk.h>
#include <string.h>

#include <storycode.h>

#include "stylesheet.h"
#include "presentation.h"
#include "frame.h"
#include "render.h"


struct renderstuff
{
	cairo_t *cr;
	PangoContext *pc;
	PangoFontMap *fontmap;
	PangoFont *font;
	struct frame *fr;
	int width;
};


static void calc_width(gpointer data, gpointer user_data)
{
	struct renderstuff *s = user_data;
	PangoItem *item = data;
	PangoGlyphString *glyphs;

	glyphs = pango_glyph_string_new();

	pango_shape(s->fr->sc+item->offset, item->length, &item->analysis,
	            glyphs);

	s->width += pango_glyph_string_get_width(glyphs);
	pango_glyph_string_free(glyphs);
}


static void render_segment(gpointer data, gpointer user_data)
{
	struct renderstuff *s = user_data;
	PangoItem *item = data;
	PangoGlyphString *glyphs;
	GdkColor col;

	glyphs = pango_glyph_string_new();

	pango_shape(s->fr->sc+item->offset, item->length, &item->analysis,
	            glyphs);

	/* FIXME: Honour alpha as well */
	gdk_color_parse("#000000", &col);
	gdk_cairo_set_source_color(s->cr, &col);
	pango_cairo_show_glyph_string(s->cr, s->font, glyphs);
	cairo_fill(s->cr);

	pango_glyph_string_free(glyphs);
	pango_item_free(item);
}


/* Render Level 1 Storycode (no subframes) */
int render_sc(struct frame *fr, cairo_t *cr, double max_w, double max_h)
{
	PangoFontDescription *fontdesc;
	struct renderstuff s;
	GList *list;
	double w, h;

	if ( fr->sc == NULL ) return 0;

	s.pc = pango_cairo_create_context(cr);
	s.fr = fr;

	/* If a minimum size is set, start there */
	if ( fr->lop.use_min_w ) w = fr->lop.min_w;
	if ( fr->lop.use_min_h ) h = fr->lop.min_h;

	/* FIXME: Handle images as well */

	/* Find and load font */
	s.fontmap = pango_cairo_font_map_get_default();
	fontdesc = pango_font_description_from_string("Serif 20");
	s.font = pango_font_map_load_font(s.fontmap, s.pc, fontdesc);
	if ( s.font == NULL ) {
		fprintf(stderr, "Failed to load font.\n");
		return 1;
	}

	/* Create glyph string */
	list = pango_itemize(s.pc, fr->sc, 0, strlen(fr->sc), NULL, NULL);
	s.width = 0;
	g_list_foreach(list, calc_width, &s);

	/* Determine width */
	w = s.width;
	if ( fr->lop.use_min_w && (s.width < fr->lop.min_w) ) {
		w = fr->lop.min_w;
	}
	if ( w > max_w ) w = max_w;

	h = 20.0;
	if ( fr->lop.use_min_h && (h < fr->lop.min_h) ) {
		h = fr->lop.min_h;
	}
	if ( h > max_h ) h = max_h;

	/* Having decided on the size, create surface and render the contents */
	if ( fr->contents != NULL ) cairo_surface_destroy(fr->contents);
	fr->contents = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(fr->contents);
	s.cr = cr;

	//cairo_rectangle(cr, 0.0, 0.0, w/2, h/2);
	//cairo_set_source_rgb(cr, 0.8, 0.8, 1.0);
	//cairo_fill(cr);

	cairo_move_to(cr, 0.0, 0.0);
	g_list_foreach(list, render_segment, &s);

	g_list_free(list);
	pango_font_description_free(fontdesc);
	g_object_unref(s.pc);

	fr->w = w;
	fr->h = h;

	return 0;
}


static void get_max_size(struct frame *fr, struct frame *parent,
                         int this_subframe, double fixed_x, double fixed_y,
                         double parent_max_width, double parent_max_height,
                         double *p_width, double *p_height)
{
	double w, h;
	int i;

	w = parent_max_width - parent->lop.pad_l - parent->lop.pad_r;
	w -= fr->lop.margin_l + fr->lop.margin_r;

	h = parent_max_height - parent->lop.pad_t - parent->lop.pad_b;
	h -= fr->lop.margin_t + fr->lop.margin_b;

	for ( i=0; i<this_subframe; i++ ) {

		//struct frame *ch;

		//ch = parent->children[i];

		/* FIXME: Shrink if this frame overlaps */
		switch ( fr->lop.grav )
		{
			case DIR_UL:
			/* Fix top left corner */
			break;

			case DIR_U:
			case DIR_UR:
			case DIR_R:
			case DIR_DR:
			case DIR_D:
			case DIR_DL:
			case DIR_L:
			fprintf(stderr, "Gravity not implemented.\n");
			break;

			case DIR_NONE:
			break;
		}
	}

	*p_width = w;
	*p_height = h;
}



static void position_frame(struct frame *fr, struct frame *parent)
{
	switch ( fr->lop.grav )
	{
		case DIR_UL:
		/* Fix top left corner */
		fr->x = parent->x + parent->lop.pad_l + fr->lop.margin_l;
		fr->y = parent->y + parent->lop.pad_t + fr->lop.margin_t;
		break;

		case DIR_U:
		case DIR_UR:
		case DIR_R:
		case DIR_DR:
		case DIR_D:
		case DIR_DL:
		case DIR_L:
		fprintf(stderr, "Gravity not implemented.\n");
		break;

		case DIR_NONE:
		break;
	}
}


static int render_frame(struct frame *fr, cairo_t *cr,
                        double max_w, double max_h)
{
	int i;

	/* Render all subframes */
	for ( i=0; i<fr->num_children; i++ ) {

		double x, y, child_max_w, child_max_h;

		/* Determine the maximum possible size this subframe can be */
		get_max_size(fr->children[i], fr, i, x, y,
		             max_w, max_h, &child_max_w, &child_max_h);

		/* Render it and hence (recursives) find out how much space it
		 * actually needs.*/
		render_frame(fr->children[i], cr, max_w, max_h);

		/* Position the frame within the parent */
		position_frame(fr->children[i], fr);

	}

	/* Render the actual contents of this frame in the remaining space */
	render_sc(fr, cr, max_w, max_h);

	return 0;
}


static void do_composite(struct frame *fr, cairo_t *cr)
{
	if ( fr->contents == NULL ) return;

	cairo_rectangle(cr, fr->x, fr->y, fr->w, fr->h);
	cairo_set_source_surface(cr, fr->contents, 0.0, 0.0);
	cairo_fill(cr);
}


static int composite_frames_at_level(struct frame *fr, cairo_t *cr,
                                     int level, int cur_level)
{
	int i;

	if ( level == cur_level ) {

		/* This frame is at the right level, so composite it */
		do_composite(fr, cr);
		return 1;

	} else {

		int n = 0;

		for ( i=0; i<fr->num_children; i++ ) {
			n += composite_frames_at_level(fr->children[i], cr,
			                               level, cur_level);
		}

		return n;

	}
}


static void composite_slide(struct slide *s, cairo_t *cr)
{
	int level = 0;
	int more = 0;

	do {
		more = composite_frames_at_level(s->top, cr, level, 0);
		level++;
	} while ( more != 0 );
}


cairo_surface_t *render_slide(struct slide *s, int w, int h)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	cairo_font_options_t *fopts;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_OFF);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_font_options(cr, fopts);

	s->top->lop.min_w = w;
	s->top->lop.min_h = h;
	s->top->lop.use_min_w = 1;
	s->top->lop.use_min_h = 1;
	render_frame(s->top, cr, w, h);

	composite_slide(s, cr);

	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);

	return surf;
}
