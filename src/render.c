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


static void render_callback_box(cairo_t *cr, struct wrap_box *box)
{
	double ascd;
	double x, y, w, h;

	cairo_surface_t *surf;

	cairo_save(cr);

	ascd = pango_units_to_double(box->ascent);

	x = 0.0;
	y = -ascd;
	cairo_user_to_device(cr, &x, &y);

	/* This is how wide the image should be in Cairo units */
	w = pango_units_to_double(box->width);
	h = pango_units_to_double(box->height);
	cairo_user_to_device_distance(cr, &w, &h);
	/* w is now how wide the image should be in pixels */

	surf = box->draw_func(w, h, box->bvp, box->vp);

	cairo_new_path(cr);
	cairo_rectangle(cr, 0.0, -ascd, pango_units_to_double(box->width),
	                                pango_units_to_double(box->height));

	if ( surf == NULL ) {
		cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
		fprintf(stderr, "Null surface box");
	} else {
		cairo_identity_matrix(cr);
		cairo_set_source_surface(cr, surf, x, y);
	}

	cairo_fill(cr);
	cairo_restore(cr);
}


static void render_image_box(cairo_t *cr, struct wrap_box *box, ImageStore *is,
                             enum is_size isz)
{
	cairo_surface_t *surf;
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

	surf = lookup_image(is, box->filename, wi, isz);
	//show_imagestore(is);

	if ( surf == NULL ) {
		cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
		fprintf(stderr, "Failed to load '%s' at size %i\n",
		        box->filename, wi);
	} else {
		cairo_identity_matrix(cr);
		cairo_set_source_surface(cr, surf, x, y);
	}

	cairo_fill(cr);
	cairo_restore(cr);
}


static void UNUSED draw_outline(cairo_t *cr, struct wrap_box *box)
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

			case WRAP_BOX_CALLBACK:
			render_callback_box(cr, box);
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


static void do_background(cairo_t *cr, struct frame *fr)
{
	cairo_pattern_t *patt = NULL;

	cairo_new_path(cr);
	cairo_rectangle(cr, 0.0, 0.0, fr->w, fr->h);

	switch ( fr->grad ) {

		case GRAD_NONE:
		cairo_set_source_rgba(cr, fr->bgcol[0],
		                          fr->bgcol[1],
		                          fr->bgcol[2],
			                  fr->bgcol[3]);
		break;

		case GRAD_VERT:
		patt = cairo_pattern_create_linear(0.0, 0.0,
			                           0.0, fr->h);
		cairo_pattern_add_color_stop_rgb(patt, 0.0, fr->bgcol[0],
			                                    fr->bgcol[1],
			                                    fr->bgcol[2]);
		cairo_pattern_add_color_stop_rgb(patt, 1.0, fr->bgcol2[0],
			                                    fr->bgcol2[1],
			                                    fr->bgcol2[2]);
		cairo_set_source(cr, patt);
		break;

		case GRAD_HORIZ:
		patt = cairo_pattern_create_linear(0.0, 0.0,
			                           fr->w, 0.0);
		cairo_pattern_add_color_stop_rgb(patt, 0.0, fr->bgcol[0],
			                                    fr->bgcol[1],
			                                    fr->bgcol[2]);
		cairo_pattern_add_color_stop_rgb(patt, 1.0, fr->bgcol2[0],
			                                    fr->bgcol2[1],
			                                    fr->bgcol2[2]);
		cairo_set_source(cr, patt);
		break;

	}

	cairo_fill(cr);
	if ( patt != NULL ) cairo_pattern_destroy(patt);
}


static int draw_frame(cairo_t *cr, struct frame *fr, ImageStore *is,
                     enum is_size isz)
{
	do_background(cr, fr);

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


static void render_sc_to_surface(SCBlock *scblocks, cairo_surface_t *surf,
                                 cairo_t *cr, double log_w, double log_h,
                                 SCBlock **stylesheets, SCCallbackList *cbl,
                                 ImageStore *is, enum is_size isz,
                                 int slide_number)
{
	PangoFontMap *fontmap;
	PangoContext *pc;
	SCInterpreter *scin;
	char snum[64];
	struct frame *top;

	cairo_rectangle(cr, 0.0, 0.0, log_w, log_h);
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

	top = sc_block_frame(scblocks);
	if ( top == NULL ) {
		top = frame_new();
		sc_block_set_frame(scblocks, top);
	}
	top->x = 0.0;
	top->y = 0.0;
	top->w = log_w;
	top->h = log_h;

	scin = sc_interp_new(pc, top);
	if ( scin == NULL ) {
		fprintf(stderr, "Failed to set up interpreter.\n");
		return;
	}

	sc_interp_set_callbacks(scin, cbl);

	snprintf(snum, 63, "%i", slide_number);
	add_macro(scin, "slidenumber", snum);

	/* "The rendering pipeline" */

	if ( stylesheets != NULL ) {
		int i = 0;
		while ( stylesheets[i] != NULL ) {
			sc_interp_run_stylesheet(scin, stylesheets[i]);
			i++;
		}
	}
	renew_frame(top);
	sc_interp_add_blocks(scin, scblocks);
	recursive_wrap_and_draw(top, cr, is, isz);

	sc_interp_destroy(scin);
	cairo_font_options_destroy(fopts);
	g_object_unref(pc);
}


/**
 * render_slide:
 * @s: A slide.
 * @w: Width of bitmap to output
 * @h: Height of bitmap to produce
 * @ww: Logical width of the rendering area
 * @hh: Logical height of the rendering area
 *
 * Render the entire slide.
 */
cairo_surface_t *render_sc(SCBlock *scblocks, int w, int h,
                           double log_w, double log_h,
                           SCBlock **stylesheets, SCCallbackList *cbl,
                           ImageStore *is, enum is_size isz,
                           int slide_number)
{
	cairo_surface_t *surf;
	cairo_t *cr;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(surf);
	cairo_scale(cr, w/log_w, h/log_h);
	render_sc_to_surface(scblocks, surf, cr, log_w, log_h,
	                     stylesheets, cbl, is, isz,slide_number);
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
		SCBlock *stylesheets[2];

		s = p->slides[i];
		stylesheets[0] = p->stylesheet;
		stylesheets[1] = NULL;

		cairo_save(cr);

		cairo_scale(cr, scale, scale);

		cairo_rectangle(cr, 0.0, 0.0, p->slide_width, p->slide_height);
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_fill(cr);

		render_sc_to_surface(s->scblocks, surf, cr, p->slide_width,
		                     p->slide_height, stylesheets, NULL,
		                     p->is, ISZ_SLIDESHOW, i);

		cairo_restore(cr);

		cairo_show_page(cr);

	}

	cairo_surface_finish(surf);
	cairo_destroy(cr);

	return 0;
}
