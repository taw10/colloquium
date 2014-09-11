/*
 * render.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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
	cairo_set_source_rgba(cr, 0.7, 0.4, 0.7, 0.2);
	cairo_fill(cr);
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
	cairo_move_to(cr, fr->w - fr->pad_l- fr->pad_r, 0.0);
	cairo_line_to(cr, fr->w - fr->pad_l - fr->pad_r,
	                  pango_units_to_double(fr->lines[i].height));
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_set_line_width(cr, 4.0);
	cairo_stroke(cr);
}


static void draw_underfull_marker(cairo_t *cr, struct frame *fr, int i)
{
	cairo_move_to(cr, fr->w - fr->pad_l- fr->pad_r, 0.0);
	cairo_line_to(cr, fr->w - fr->pad_l - fr->pad_r,
	                  pango_units_to_double(fr->lines[i].height));
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 4.0);
	cairo_stroke(cr);
}


static void render_lines(struct frame *fr, cairo_t *cr, ImageStore *is,
                         enum is_size isz)
{
	int i;
	double y_pos = 0.0;
	const int debug = 0;

	for ( i=0; i<fr->n_lines; i++ ) {

		cairo_save(cr);

		/* Move to beginning of the line */
		cairo_translate(cr, 0.0, y_pos);

		if ( debug ) {
			cairo_move_to(cr,
			        0.0,
				0.5+pango_units_to_double(fr->lines[i].ascent));
			cairo_line_to(cr,
			        pango_units_to_double(fr->lines[i].width),
				0.5+pango_units_to_double(fr->lines[i].ascent));
			cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
			cairo_set_line_width(cr, 1.0);
			cairo_stroke(cr);

			cairo_move_to(cr, 0.0, 0.0);
			cairo_line_to(cr, 0.0,
			            pango_units_to_double(fr->lines[i].height));
			cairo_set_line_width(cr, 3.0);
			cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
			cairo_stroke(cr);
		}

		/* Render the line */
		cairo_save(cr);
		cairo_translate(cr, 0.0,
		                    pango_units_to_double(fr->lines[i].ascent));
		render_boxes(&fr->lines[i], cr, is, isz);
		cairo_restore(cr);

		if ( debug && fr->lines[i].overfull ) {
			draw_overfull_marker(cr, fr, i);
		}
		if ( debug && fr->lines[i].underfull ) {
			draw_underfull_marker(cr, fr, i);
		}

		/* FIXME: line spacing */
		y_pos += pango_units_to_double(fr->lines[i].height);

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


static int recursive_wrap_and_draw(struct frame *fr, cairo_t *cr,
                                   ImageStore *is, enum is_size isz)
{
	int i;

	/* Wrap boxes -> wrap lines */
	wrap_contents(fr);

	/* Actually draw the lines */
	draw_frame(cr, fr, is, isz);

	for ( i=0; i<fr->num_children; i++ ) {
		cairo_save(cr);
		cairo_translate(cr, fr->children[i]->x, fr->children[i]->y);
		recursive_wrap_and_draw(fr->children[i], cr, is, isz);
		cairo_restore(cr);
	}

	return 0;
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



static void render_slide_to_surface(struct slide *s, cairo_surface_t *surf,
                                    cairo_t *cr,  enum is_size isz,
                                    double scale, double w, double h,
                                    ImageStore *is, int slide_number)
{
	PangoFontMap *fontmap;
	PangoContext *pc;
	SCInterpreter *scin;
	char snum[64];

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

	scin = sc_interp_new(pc, s->top);
	if ( scin == NULL ) {
		fprintf(stderr, "Failed to set up interpreter.\n");
		return;
	}

	snprintf(snum, 63, "%i", slide_number);
	add_macro(scin, "slidenumber", snum);

	/* "The rendering pipeline" */
	sc_interp_run_stylesheet(scin, s->parent->stylesheet);
	renew_frame(s->top);
	sc_interp_add_blocks(scin, s->scblocks);
	recursive_wrap_and_draw(s->top, cr, is, isz);

	sc_interp_destroy(scin);
	cairo_font_options_destroy(fopts);
	g_object_unref(pc);
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
                              ImageStore *is, enum is_size isz,
                              int slide_number)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int h;
	double scale;

	h = (hh/ww)*w;
	scale = w/ww;

	s->top->x = 0.0;
	s->top->y = 0.0;
	s->top->w = ww;
	s->top->h = hh;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(surf);
	render_slide_to_surface(s, surf, cr, isz, scale, w, h, is,
	                        slide_number);
	cairo_destroy(cr);
	return surf;
}


int export_pdf(struct presentation *p, const char *filename)
{
	int i;
	double r;
	double w = 2048.0;
	double scale;
	cairo_surface_t *surf;
	cairo_t *cr;

	r = p->slide_height / p->slide_width;

	surf = cairo_pdf_surface_create(filename, w, w*r);
	if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
		fprintf(stderr, "Couldn't create Cairo surface\n");
		return 1;
	}

	cr = cairo_create(surf);
	scale = w / p->slide_width;

	for ( i=0; i<p->num_slides; i++ ) {

		struct slide *s;

		s = p->slides[i];

		cairo_save(cr);

		cairo_rectangle(cr, 0.0, 0.0, p->slide_width, p->slide_height);
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_fill(cr);

		s->top->x = 0.0;
		s->top->y = 0.0;
		s->top->w = w;
		s->top->h = w*r;

		render_slide_to_surface(s, surf, cr, ISZ_SLIDESHOW, scale,
		                        w, w*r, p->is, i);

		cairo_restore(cr);

		cairo_show_page(cr);

	}

	cairo_surface_finish(surf);
	cairo_destroy(cr);

	return 0;
}
