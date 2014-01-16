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

#include "sc_parse.h"
#include "sc_interp.h"
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
                             enum is_size isz)
{
	GdkPixbuf *pixbuf;
	double w, h;
	int wi;
	double ascd;
	double x, y;

	cairo_save(cr);

	ascd = pango_units_to_double(box->ascent);

	x = 0.0;
	y = -ascd;
	cairo_user_to_device(cr, &x, &y);

	cairo_new_path(cr);
	cairo_rectangle(cr, 0.0, -ascd,
	                    pango_units_to_double(box->width),
	                    pango_units_to_double(box->height));

	/* This is how wide the image should be in Cairo units */
	w = pango_units_to_double(box->width);

	h = 0.0;  /* Dummy */
	cairo_user_to_device_distance(cr, &w, &h);
	/* w is now how wide the image should be in pixels */

	wi = lrint(w);

	pixbuf = lookup_image(is, box->filename, wi, isz);
	//show_imagestore(is);

	if ( pixbuf == NULL ) {
		cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
		fprintf(stderr, "Failed to load '%s' at size %i\n",
		        box->filename, wi);
	} else {
		cairo_identity_matrix(cr);
		gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
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
                         enum is_size isz)
{
	int j;
	double x_pos = 0.0;

	for ( j=0; j<line->n_boxes; j++ ) {

		struct wrap_box *box;

		cairo_save(cr);

		box = &line->boxes[j];
		cairo_translate(cr, x_pos, 0.0);

		//draw_outline(cr, box);

		switch ( line->boxes[j].type ) {

			case WRAP_BOX_PANGO :
			render_glyph_box(cr, box);
			break;

			case WRAP_BOX_IMAGE :
			render_image_box(cr, box, is, isz);
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
#if 0
	cairo_move_to(cr, fr->w - fr->lop.pad_l- fr->lop.pad_r, 0.0);
	cairo_line_to(cr, fr->w - fr->lop.pad_l - fr->lop.pad_r,
	                  pango_units_to_double(fr->lines[i].height));
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_set_line_width(cr, 4.0);
	cairo_stroke(cr);
#endif
}


static void draw_underfull_marker(cairo_t *cr, struct frame *fr, int i)
{
#if 0
	cairo_move_to(cr, fr->w - fr->lop.pad_l- fr->lop.pad_r, 0.0);
	cairo_line_to(cr, fr->w - fr->lop.pad_l - fr->lop.pad_r,
	                  pango_units_to_double(fr->lines[i].height));
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 4.0);
	cairo_stroke(cr);
#endif
}


static void render_lines(struct frame *fr, cairo_t *cr, ImageStore *is,
                         enum is_size isz)
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
		render_boxes(&fr->lines[i], cr, is, isz);
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


static int draw_frame(cairo_t *cr, struct frame *fr, ImageStore *is,
                     enum is_size isz)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, 0.0, 0.0, fr->w, fr->h);
	cairo_set_source_rgba(cr, fr->bgcol[0], fr->bgcol[1], fr->bgcol[2],
	                          fr->bgcol[3]);
	cairo_fill(cr);

	if ( fr->trouble ) {
		cairo_new_path(cr);
		cairo_rectangle(cr, 0.0, 0.0, fr->w, fr->h);
		cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
		cairo_set_line_width(cr, 2.0);
		cairo_stroke(cr);
	}

	/* Actually render the lines */
	cairo_translate(cr, fr->pad_l, fr->pad_t);
	render_lines(fr, cr, is, isz);

	return 0;
}


static int render_frame(cairo_t *cr, struct frame *fr, ImageStore *is,
                        enum is_size isz, struct slide_constants *scc,
		        struct presentation_constants *pcc,
			PangoContext *pc)
{
	SCInterpreter *scin;
	int i;
	SCBlock *bl = fr->scblocks;

	scin = sc_interp_new(pc, fr);
	if ( scin == NULL ) {
		fprintf(stderr, "Failed to set up interpreter.\n");
		return 1;
	}

	for ( i=0; i<fr->n_lines; i++ ) {
		wrap_line_free(&fr->lines[i]);
	}
	free(fr->lines);
	fr->lines = NULL;
	fr->n_lines = 0;
	fr->max_lines = 0;

	/* SCBlocks -> frames and wrap boxes (preferably re-using frames) */
	sc_interp_add_blocks(scin, bl);

	/* Wrap boxes -> wrap lines */
	wrap_contents(fr, sc_interp_get_boxes(scin));

	/* Actually draw the lines */
	draw_frame(cr, fr, is, isz);

	for ( i=0; i<fr->num_children; i++ ) {
		cairo_translate(cr, fr->children[i]->x, fr->children[i]->y);
		render_frame(cr, fr->children[i], is, isz, scc, pcc, pc);
		cairo_restore(cr);
	}

	sc_interp_destroy(scin);

	return 0;
}


void recursive_buffer_free(struct frame *fr)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {
		recursive_buffer_free(fr->children[i]);
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
	PangoFontMap *fontmap;
	PangoContext *pc;

	h = (hh/ww)*w;
	scale = w/ww;

	s->top->x = 0.0;
	s->top->y = 0.0;
	s->top->w = ww;
	s->top->h = hh;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(surf);

	cairo_scale(cr, scale, scale);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_font_options_t *fopts;
	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_DEFAULT);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_GRAY);
	cairo_set_font_options(cr, fopts);

	/* Find and load font */
	fontmap = pango_cairo_font_map_get_default();
	pc = pango_font_map_create_context(fontmap);
	pango_cairo_update_context(cr, pc);

	render_frame(cr, s->top, is, isz, s->constants,
	             s->parent->constants, pc);

	cairo_font_options_destroy(fopts);
	g_object_unref(pc);
	cairo_destroy(cr);
	recursive_buffer_free(s->top);

	return surf;
}


int export_pdf(struct presentation *p, const char *filename)
{
	int i;
	double r;
	double w = 2048.0;
	cairo_surface_t *surf;
	cairo_t *cr;
	double scale;
	PangoFontMap *fontmap;
	PangoContext *pc;

	r = p->slide_height / p->slide_width;

	surf = cairo_pdf_surface_create(filename, w, w*r);

	if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
		fprintf(stderr, "Couldn't create Cairo surface\n");
		return 1;
	}

	cr = cairo_create(surf);

	scale = w / p->slide_width;
	cairo_scale(cr, scale, scale);

	cairo_font_options_t *fopts;
	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_DEFAULT);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_GRAY);
	cairo_set_font_options(cr, fopts);

	/* Find and load font */
	fontmap = pango_cairo_font_map_get_default();
	pc = pango_font_map_create_context(fontmap);
	pango_cairo_update_context(cr, pc);

	for ( i=0; i<p->num_slides; i++ ) {

		struct slide *s;

		s = p->slides[i];
		cairo_rectangle(cr, 0.0, 0.0, p->slide_width, p->slide_height);
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_fill(cr);

		s->top->x = 0.0;
		s->top->y = 0.0;
		s->top->w = w;
		s->top->h = w*r;

		render_frame(cr, s->top, p->is, ISZ_SLIDESHOW, s->constants,
		             s->parent->constants, pc);

		cairo_show_page(cr);

	}

	cairo_surface_finish(surf);
	cairo_font_options_destroy(fopts);
	g_object_unref(pc);
	cairo_destroy(cr);

	return 0;
}
