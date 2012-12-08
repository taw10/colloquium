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
#include <stdlib.h>

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
	PangoLogAttr *log_attrs;
	struct frame *fr;
	double width;
	double height;
	double ascent;
};


static void calc_width(gpointer data, gpointer user_data)
{
	struct renderstuff *s = user_data;
	PangoItem *item = data;
	PangoGlyphString *glyphs;
	PangoRectangle extents;

	glyphs = pango_glyph_string_new();

	pango_shape(s->fr->sc+item->offset, item->length, &item->analysis,
	            glyphs);

	pango_glyph_string_extents_range(glyphs, item->offset,
	                                 item->offset+item->length,
	                                 item->analysis.font,
	                                 &extents, NULL);

	s->width += extents.width;
	s->height = extents.height;  /* FIXME: Total rubbish */
	s->ascent = PANGO_ASCENT(extents);

	pango_glyph_string_free(glyphs);
}


static void render_segment(gpointer data, gpointer user_data)
{
	struct renderstuff *s = user_data;
	PangoItem *item = data;
	PangoGlyphString *glyphs;
	PangoGlyphItem gitem;
	GdkColor col;

	glyphs = pango_glyph_string_new();

	pango_shape(s->fr->sc+item->offset, item->length, &item->analysis,
	            glyphs);

	gitem.item = item;
	gitem.glyphs = glyphs;

	/* FIXME: Honour alpha as well */
	gdk_color_parse("#000000", &col);
	gdk_cairo_set_source_color(s->cr, &col);
	cairo_set_source_rgb(s->cr, 0.0, 1.0, 0.0);
	pango_cairo_show_glyph_item(s->cr, s->fr->sc+item->offset, &gitem);

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
	int len;
	PangoAttrList *attrs;
	PangoAttribute *attr_font;

	if ( fr->sc == NULL ) return 0;

	s.pc = pango_cairo_create_context(cr);
	s.fr = fr;

	/* If a minimum size is set, start there */
	if ( fr->lop.use_min_w ) w = fr->lop.min_w;
	if ( fr->lop.use_min_h ) h = fr->lop.min_h;

	/* FIXME: Handle images as well */

	/* Find and load font */
	s.fontmap = pango_cairo_font_map_get_default();
	fontdesc = pango_font_description_from_string("Sans 24");

	attrs = pango_attr_list_new();
	attr_font = pango_attr_font_desc_new(fontdesc);
	pango_attr_list_insert_before(attrs, attr_font);
	pango_font_description_free(fontdesc);

	len = strlen(fr->sc);
	s.log_attrs = calloc(len+1, sizeof(PangoLogAttr));
	if ( s.log_attrs == NULL ) {
		fprintf(stderr, "Failed to allocate memory for log attrs\n");
		return 1;
	}
	pango_get_log_attrs(fr->sc, len, -1,
	                    pango_language_get_default(), s.log_attrs, len+1);

	/* Create glyph string */
	list = pango_itemize(s.pc, fr->sc, 0, strlen(fr->sc), attrs, NULL);
	s.width = 0;
	g_list_foreach(list, calc_width, &s);

	/* Determine width */
	w = s.width / PANGO_SCALE;
	w += fr->lop.pad_l + fr->lop.pad_r;
	if ( fr->lop.use_min_w && (s.width < fr->lop.min_w) ) {
		w = fr->lop.min_w;
	}
	if ( w > max_w ) w = max_w;

	h = s.height / PANGO_SCALE;
	h += fr->lop.pad_t + fr->lop.pad_b;
	if ( fr->lop.use_min_h && (h < fr->lop.min_h) ) {
		h = fr->lop.min_h;
	}
	if ( h > max_h ) h = max_h;

	/* Having decided on the size, create surface and render the contents */
	if ( fr->contents != NULL ) cairo_surface_destroy(fr->contents);
	fr->contents = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(fr->contents);
	s.cr = cr;

	cairo_rectangle(cr, w/2, h/2, w/2, h/2);
	cairo_set_source_rgb(cr, 0.0, 0.8, 1.0);
	cairo_fill(cr);

	cairo_move_to(cr, fr->lop.pad_l, fr->lop.pad_t + s.ascent/PANGO_SCALE);
	g_list_foreach(list, render_segment, &s);

	cairo_destroy(cr);
	g_list_free(list);
	free(s.log_attrs);
	pango_attr_list_unref(attrs);
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
		case DIR_NONE:
		fprintf(stderr, "Gravity not implemented.\n");
		break;

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
		struct frame *ch = fr->children[i];

		if ( ch->style != NULL ) {
			memcpy(&ch->lop, &ch->style->lop,
			       sizeof(struct layout_parameters));
		}

		/* Determine the maximum possible size this subframe can be */
		get_max_size(ch, fr, i, x, y,
		             max_w, max_h, &child_max_w, &child_max_h);

		/* Render it and hence (recursives) find out how much space it
		 * actually needs.*/
		render_frame(ch, cr, child_max_w, child_max_h);

		/* Position the frame within the parent */
		position_frame(ch, fr);

	}

	/* Render the actual contents of this frame in the remaining space */
	render_sc(fr, cr, max_w, max_h);

	return 0;
}


void recursive_buffer_free(struct frame *fr)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {
		recursive_buffer_free(fr->children[i]);
	}

	if ( fr->contents != NULL ) {
		cairo_surface_destroy(fr->contents);
		fr->contents = NULL;
	}
}


void free_render_buffers(struct slide *s)
{
	recursive_buffer_free(s->top);
	if ( s->rendered_edit != NULL ) cairo_surface_destroy(s->rendered_edit);
	if ( s->rendered_proj != NULL ) cairo_surface_destroy(s->rendered_proj);
	if ( s->rendered_thumb != NULL ) {
		cairo_surface_destroy(s->rendered_thumb);
	}
}


static void do_composite(struct frame *fr, cairo_t *cr)
{
	if ( fr->contents == NULL ) return;

	cairo_rectangle(cr, fr->x, fr->y, fr->w, fr->h);
	cairo_set_source_surface(cr, fr->contents, fr->x, fr->y);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke(cr);
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
			                               level, cur_level+1);
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
