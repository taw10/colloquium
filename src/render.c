/*
 * render.c
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

#include "storycode.h"
#include "stylesheet.h"
#include "presentation.h"
#include "frame.h"
#include "render.h"
#include "wrap.h"
#include "imagestore.h"


static void render_glyph_box(cairo_t *cr, struct wrap_box *box)
{
	cairo_new_path(cr);
	cairo_move_to(cr, 0.0, 0.0);
	if ( box->glyphs == NULL ) {
		fprintf(stderr, "Box %p has NULL pointer.\n", box);
		return;
	}
	pango_cairo_glyph_string_path(cr, box->font, box->glyphs);
	cairo_set_source_rgba(cr, box->col[0], box->col[1], box->col[2],
	                      box->col[3]);
	cairo_fill(cr);
}



static void render_image_box(cairo_t *cr, struct wrap_box *box, ImageStore *is,
                             enum is_size isz, double scale)
{
	GdkPixbuf *pixbuf;
	int w;
	double ascd;

	cairo_save(cr);

	ascd = pango_units_to_double(box->ascent);

	cairo_new_path(cr);
	cairo_scale(cr, 1.0/scale, 1.0/scale);
	cairo_rectangle(cr, 0.0, -scale*ascd,
	                    scale*pango_units_to_double(box->width),
	                    scale*pango_units_to_double(box->height));

	w = lrint(pango_units_to_double(scale*box->width));
	pixbuf = lookup_image(is, box->filename, w, isz);
	//show_imagestore(is);

	if ( pixbuf == NULL ) {
		cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
		fprintf(stderr, "Failed to load '%s' at size %i\n",
		        box->filename, w);
	} else {
		gdk_cairo_set_source_pixbuf(cr, pixbuf, 0.0, -scale*ascd);
	}

	cairo_fill(cr);
	cairo_restore(cr);
}


static void draw_outline(cairo_t *cr, struct wrap_box *box)
{
	char tmp[32];
	double asc, desc;

	if ( box->type == WRAP_BOX_SENTINEL ) return;

	asc = pango_units_to_double(box->ascent);
	desc = pango_units_to_double(box->height) - asc;

	cairo_rectangle(cr, 0.0, -asc, pango_units_to_double(box->width),
			         asc + desc);
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.1);
	cairo_stroke(cr);

	cairo_rectangle(cr, pango_units_to_double(box->width), -asc,
			pango_units_to_double(box->sp), asc + desc);
	cairo_set_source_rgb(cr, 0.7, 0.4, 0.7);
	cairo_fill(cr);

	snprintf(tmp, 31, "%lli", (long long int)box->sc_offset);
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_ITALIC,
	                                   CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10.0);
	cairo_move_to(cr, 0.0, desc);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.7);
	cairo_show_text(cr, tmp);
}


static void render_boxes(struct wrap_line *line, cairo_t *cr, ImageStore *is,
                         enum is_size isz, double scale)
{
	int j;
	double x_pos = 0.0;

	for ( j=0; j<line->n_boxes; j++ ) {

		struct wrap_box *box;

		cairo_save(cr);

		box = &line->boxes[j];
		cairo_translate(cr, x_pos, 0.0);

		draw_outline(cr, box);

		switch ( line->boxes[j].type ) {

			case WRAP_BOX_PANGO :
			render_glyph_box(cr, box);
			break;

			case WRAP_BOX_IMAGE :
			render_image_box(cr, box, is, isz, scale);
			break;

			case WRAP_BOX_NOTHING :
			break;

			case WRAP_BOX_SENTINEL :
			/* Do nothing */
			break;

		}

		x_pos += pango_units_to_double(line->boxes[j].width);
		x_pos += pango_units_to_double(line->boxes[j].sp);

		cairo_restore(cr);

	}
}


static void draw_overfull_marker(cairo_t *cr, struct frame *fr, int i)
{
	cairo_move_to(cr, fr->w - fr->lop.pad_l- fr->lop.pad_r, 0.0);
	cairo_line_to(cr, fr->w - fr->lop.pad_l - fr->lop.pad_r,
	                  pango_units_to_double(fr->lines[i].height));
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_set_line_width(cr, 4.0);
	cairo_stroke(cr);
}


static void draw_underfull_marker(cairo_t *cr, struct frame *fr, int i)
{
	cairo_move_to(cr, fr->w - fr->lop.pad_l- fr->lop.pad_r, 0.0);
	cairo_line_to(cr, fr->w - fr->lop.pad_l - fr->lop.pad_r,
	                  pango_units_to_double(fr->lines[i].height));
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 4.0);
	cairo_stroke(cr);
}


static void render_lines(struct frame *fr, cairo_t *cr, ImageStore *is,
                         enum is_size isz, double scale)
{
	int i;
	double y_pos = 0.0;

	for ( i=0; i<fr->n_lines; i++ ) {

		cairo_save(cr);

		/* Move to beginning of the line */
		cairo_translate(cr, 0.0, y_pos);

		#if 0
		cairo_move_to(cr, 0.0,
		                0.5+pango_units_to_double(fr->lines[i].ascent));
		cairo_line_to(cr, pango_units_to_double(fr->lines[i].width),
		                0.5+pango_units_to_double(fr->lines[i].ascent));
		cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);
		#endif

		/* Render the line */
		cairo_save(cr);
		cairo_translate(cr, 0.0,
		                    pango_units_to_double(fr->lines[i].ascent));
		render_boxes(&fr->lines[i], cr, is, isz, scale);
		cairo_restore(cr);

		if ( fr->lines[i].overfull ) {
			draw_overfull_marker(cr, fr, i);
		}
		if ( fr->lines[i].underfull ) {
			draw_underfull_marker(cr, fr, i);
		}

		/* FIXME: line spacing */
		y_pos += pango_units_to_double(fr->lines[i].height) + 0.0;

		cairo_restore(cr);

	}
}


static void run_render_sc(cairo_t *cr, struct frame *fr, const char *sc)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;

	bl = sc_find_blocks(sc, "bgcol");
	if ( bl == NULL ) return;

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		GdkColor col;

		if ( b->contents == NULL ) continue;
		gdk_color_parse(b->contents, &col);
		cairo_rectangle(cr, 0, 0, fr->w, fr->h);
		gdk_cairo_set_source_color(cr, &col);
		cairo_fill(cr);

	}
	sc_block_list_free(bl);
}



static void do_background(cairo_t *cr, struct frame *fr)
{
	if ( fr->style != NULL ) {
		run_render_sc(cr, fr, fr->style->sc_prologue);
	}
	run_render_sc(cr, fr, fr->sc);
}


/* Render Level 1 Storycode (no subframes) */
static int render_sc(struct frame *fr, double scale, ImageStore *is,
                     enum is_size isz)
{
	int i;
	cairo_t *cr;
	PangoFontMap *fontmap;
	PangoContext *pc;

	for ( i=0; i<fr->n_lines; i++ ) {
		wrap_line_free(&fr->lines[i]);
	}
	free(fr->lines);
	fr->lines = NULL;
	fr->n_lines = 0;
	fr->max_lines = 0;

	if ( fr->sc == NULL ) return 0;

	/* Create surface and Cairo stuff */
	if ( fr->contents != NULL ) cairo_surface_destroy(fr->contents);
	/* Rounding to get the bitmap size */
	fr->contents = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                          fr->pix_w, fr->pix_h);
	cr = cairo_create(fr->contents);
	cairo_scale(cr, scale, scale);

	do_background(cr, fr);

	cairo_font_options_t *fopts;
	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_DEFAULT);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_GRAY);
	cairo_set_font_options(cr, fopts);

	/* Find and load font */
	fontmap = pango_cairo_font_map_get_default();
	pc = pango_font_map_create_context(fontmap);
	pango_cairo_update_context(cr, pc);

	/* Set up lines */
	if ( wrap_contents(fr, pc) ) {
		fprintf(stderr, "Failed to wrap lines.\n");
		return 1;
	}

	/* Actually render the lines */
	cairo_translate(cr, fr->lop.pad_l, fr->lop.pad_t);
	render_lines(fr, cr, is, isz, scale);

	/* Tidy up */
	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);
	g_object_unref(pc);

	return 0;
}


static int render_frame(struct frame *fr, double scale, ImageStore *is,
                        enum is_size isz)
{
	int i;

	/* Render all subframes */
	for ( i=0; i<fr->num_children; i++ ) {

		struct frame *ch = fr->children[i];
		double mtot;

		if ( (ch->style != NULL) && ch->lop_from_style ) {
			memcpy(&ch->lop, &ch->style->lop,
			       sizeof(struct layout_parameters));
		}


		mtot = ch->lop.margin_l + ch->lop.margin_r;
		mtot += fr->lop.pad_l + fr->lop.pad_r;
		switch ( ch->lop.w_units ) {

			case UNITS_SLIDE :
			ch->w = ch->lop.w;
			break;

			case UNITS_FRAC :
			ch->w = fr->w * ch->lop.w - mtot;
			break;

		}

		mtot = ch->lop.margin_t + ch->lop.margin_b;
		mtot += fr->lop.pad_t + fr->lop.pad_b;
		switch ( ch->lop.h_units ) {

			case UNITS_SLIDE :
			ch->h = ch->lop.h;
			break;

			case UNITS_FRAC :
			ch->h = fr->h * ch->lop.h - mtot;
			break;

		}

		/* Rounding to get bitmap size */
		ch->pix_w =  ch->w*scale;
		ch->pix_h = ch->h*scale;
		render_frame(ch, scale, is, isz);

		ch->x = ch->lop.x + fr->lop.pad_l + ch->lop.margin_l;
		ch->y = ch->lop.y + fr->lop.pad_t + ch->lop.margin_t;

	}

	render_sc(fr, scale, is, isz);

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


static void do_composite(struct frame *fr, cairo_t *cr, double scale)
{
	if ( fr->contents == NULL ) return;

	cairo_rectangle(cr, scale*fr->x, scale*fr->y, fr->pix_w, fr->pix_h);
	cairo_set_source_surface(cr, fr->contents, scale*fr->x, scale*fr->y);
	cairo_fill(cr);

//	cairo_rectangle(cr, scale*fr->x, scale*fr->y, fr->pix_w, fr->pix_h);
//	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
//	cairo_stroke(cr);
}


static int composite_frames_at_level(struct frame *fr, cairo_t *cr,
                                     int level, int cur_level, double scale)
{
	int i;

	if ( level == cur_level ) {

		/* This frame is at the right level, so composite it */
		do_composite(fr, cr, scale);
		return 1;

	} else {

		int n = 0;

		for ( i=0; i<fr->num_children; i++ ) {
			n += composite_frames_at_level(fr->children[i], cr,
			                               level, cur_level+1,
			                               scale);
		}

		return n;

	}
}


static void composite_slide(struct slide *s, cairo_t *cr, double scale)
{
	int level = 0;
	int more = 0;

	do {
		more = composite_frames_at_level(s->top, cr, level, 0, scale);
		level++;
	} while ( more != 0 );
}


/**
 * render_slide:
 * @s: A slide.
 * @w: Width of the bitmap to produce
 * @ww: Width of the slide in Cairo units
 * @hh: Height of the slide in Cairo units
 *
 * Render the entire slide.
 */
cairo_surface_t *render_slide(struct slide *s, int w, double ww, double hh,
                              ImageStore *is, enum is_size isz)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int h;
	double scale;

	h = (hh/ww)*w;
	scale = w/ww;

	if ( s->top->style != NULL ) {
		memcpy(&s->top->lop, &s->top->style->lop,
		       sizeof(struct layout_parameters));
	}
	s->top->lop.x = 0.0;
	s->top->lop.y = 0.0;
	s->top->lop.w = ww;
	s->top->lop.h = hh;
	s->top->w = ww;
	s->top->h = hh;
	s->top->pix_w = w;
	s->top->pix_h = h;
	render_frame(s->top, scale, is, isz);

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	//show_heirarchy(s->top, "");

	composite_slide(s, cr, scale);

	cairo_destroy(cr);
	recursive_buffer_free(s->top);

	return surf;
}


int export_pdf(struct presentation *p, const char *filename)
{
	int i;
	cairo_surface_t *surf;
	cairo_t *cr;

	surf = cairo_pdf_surface_create(filename, p->slide_width,
	                                          p->slide_height);

	if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
		fprintf(stderr, "Couldn't create Cairo surface\n");
		return 1;
	}

	cr = cairo_create(surf);

	for ( i=0; i<p->num_slides; i++ ) {

		cairo_surface_t *sf;
		struct slide *s;

		s = p->slides[i];
		sf = render_slide(s, 1024, p->slide_width, p->slide_height,
		                  p->is, ISZ_SLIDESHOW);
		cairo_rectangle(cr, 0.0, 0.0, p->slide_width, p->slide_height);
		cairo_set_source_surface(cr, sf, 0.0, 0.0);
		cairo_fill(cr);
		cairo_surface_show_page(surf);
		cairo_surface_destroy(sf);

	}

	cairo_surface_finish(surf);
	cairo_destroy(cr);

	return 0;
}
