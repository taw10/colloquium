/*
 * render.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2013 Thomas White <taw@bitwiz.org.uk>
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
#include <string.h>
#include <stdlib.h>

#include <storycode.h>

#include "stylesheet.h"
#include "presentation.h"
#include "frame.h"
#include "render.h"
#include "wrap.h"


static void render_glyph_box(cairo_t *cr, struct wrap_box *box)
{
	cairo_new_path(cr);
	cairo_move_to(cr, 0.0, pango_units_to_double(box->ascent));
	if ( box->glyphs == NULL ) {
		fprintf(stderr, "Box %p has NULL pointer.\n", box);
		return;
	}
	pango_cairo_glyph_string_path(cr, box->font, box->glyphs);
	cairo_set_source_rgba(cr, box->col[0], box->col[1], box->col[2],
	                      box->col[3]);
	cairo_fill(cr);

	cairo_rectangle(cr, 0.0, 0.0, pango_units_to_double(box->width),
	                              pango_units_to_double(box->height));
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.1);
	cairo_stroke(cr);
}


static void render_boxes(struct wrap_line *line, cairo_t *cr)
{
	int j;
	double x_pos = 0.0;

	for ( j=0; j<line->n_boxes; j++ ) {

		struct wrap_box *box;

		cairo_save(cr);

		box = &line->boxes[j];
		cairo_translate(cr, x_pos, 0.0);

		switch ( line->boxes[j].type ) {

			case WRAP_BOX_PANGO :
			render_glyph_box(cr, box);
			break;

			case WRAP_BOX_IMAGE :
			/* FIXME ! */
			break;

		}

		x_pos += pango_units_to_double(line->boxes[j].width);

		cairo_restore(cr);

	}
}


static void render_lines(struct frame *fr, cairo_t *cr)
{
	int i;
	double y_pos = 0.0;

	for ( i=0; i<fr->n_lines; i++ ) {

		double asc = pango_units_to_double(fr->lines[i].ascent);

		cairo_move_to(cr, 0, y_pos+asc+0.5);
		cairo_line_to(cr, pango_units_to_double(fr->lines[i].width),
		                  y_pos+asc+0.5);
		cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);

		/* Move to beginning of the line */
		cairo_move_to(cr, 0.0, asc+y_pos);

		/* Render the line */
		render_boxes(&fr->lines[i], cr);

		/* FIXME: line spacing */
		y_pos += pango_units_to_double(fr->lines[i].height) + 0.0;

	}
}


static void free_line_bits(struct wrap_line *l)
{
	int i;
	for ( i=0; i<l->n_boxes; i++ ) {

		switch ( l->boxes[i].type ) {

			case WRAP_BOX_PANGO :
			pango_glyph_string_free(l->boxes[i].glyphs);
			free(l->boxes[i].text);
			break;

			case WRAP_BOX_IMAGE :
			break;

		}

	}

	free(l->boxes);
}


/* Render Level 1 Storycode (no subframes) */
static int render_sc(struct frame *fr)
{
	int i;
	cairo_t *cr;
	PangoFontMap *fontmap;
	PangoContext *pc;

	for ( i=0; i<fr->n_lines; i++ ) {
		free_line_bits(&fr->lines[i]);
	}
	free(fr->lines);
	fr->lines = NULL;
	fr->n_lines = 0;
	fr->max_lines = 0;

	if ( fr->sc == NULL ) return 0;

	/* Create surface and Cairo stuff */
	if ( fr->contents != NULL ) cairo_surface_destroy(fr->contents);
	fr->contents = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                          fr->w, fr->h);
	cr = cairo_create(fr->contents);

	cairo_font_options_t *fopts;
	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_DEFAULT);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_GRAY);
	cairo_set_font_options(cr, fopts);

	/* Find and load font */
	fontmap = pango_cairo_font_map_get_default();
	pc = pango_font_map_create_context(fontmap);

	/* Set up lines */
	if ( wrap_contents(fr, pc) ) {
		fprintf(stderr, "Failed to wrap lines.\n");
		return 1;
	}

	/* Actually render the lines */
	cairo_translate(cr, fr->lop.pad_l, fr->lop.pad_t);
	render_lines(fr, cr);

	/* Tidy up */
	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);
	g_object_unref(pc);

	return 0;
}


static int render_frame(struct frame *fr, cairo_t *cr)
{
	if ( fr->num_children > 0 ) {

		int i;

		/* Render all subframes */
		for ( i=0; i<fr->num_children; i++ ) {

			struct frame *ch = fr->children[i];
			double mtot;

			if ( ch->style != NULL ) {
				memcpy(&ch->lop, &ch->style->lop,
				       sizeof(struct layout_parameters));
			}


			mtot = ch->lop.margin_l + ch->lop.margin_r;
			switch ( ch->lop.w_units ) {

				case UNITS_SLIDE :
				ch->w = ch->lop.w - mtot;
				break;

				case UNITS_FRAC :
				ch->w = fr->w * ch->lop.w - mtot;
				break;

			}

			mtot = ch->lop.margin_t + ch->lop.margin_b;
			switch ( ch->lop.h_units ) {

				case UNITS_SLIDE :
				ch->h = ch->lop.h - mtot;
				break;

				case UNITS_FRAC :
				ch->h = fr->h * ch->lop.h - mtot;
				break;

			}

			render_frame(ch, cr);

			ch->x = ch->lop.x + ch->lop.margin_l;
			ch->y = ch->lop.y + ch->lop.margin_t;

		}

	} else {

		render_sc(fr);

	}

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
	if ( s->rendered_edit != NULL ) cairo_surface_destroy(s->rendered_edit);
	if ( s->rendered_proj != NULL ) cairo_surface_destroy(s->rendered_proj);
	if ( s->rendered_thumb != NULL ) {
		cairo_surface_destroy(s->rendered_thumb);
	}

	s->rendered_edit = NULL;
	s->rendered_proj = NULL;
	s->rendered_thumb = NULL;
}


void free_render_buffers_except_thumb(struct slide *s)
{
	if ( s->rendered_edit != NULL ) cairo_surface_destroy(s->rendered_edit);
	if ( s->rendered_proj != NULL ) cairo_surface_destroy(s->rendered_proj);

	s->rendered_edit = NULL;
	s->rendered_proj = NULL;
}



static void do_composite(struct frame *fr, cairo_t *cr)
{
	if ( fr->contents == NULL ) return;

	cairo_save(cr);
	cairo_rectangle(cr, fr->x, fr->y, fr->w, fr->h);
	cairo_clip(cr);
	cairo_set_source_surface(cr, fr->contents, fr->x, fr->y);
	cairo_paint(cr);
	cairo_restore(cr);

//	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
//	cairo_rectangle(cr, fr->x+0.5, fr->y+0.5, fr->w, fr->h);
//	cairo_stroke(cr);
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

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	s->top->lop.x = 0.0;
	s->top->lop.y = 0.0;
	s->top->lop.w = w;
	s->top->lop.h = h;
	s->top->w = w;
	s->top->h = h;
	render_frame(s->top, cr);

	composite_slide(s, cr);

	cairo_destroy(cr);
	recursive_buffer_free(s->top);

	return surf;
}
