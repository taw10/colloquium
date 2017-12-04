/*
 * render.c
 *
 * Copyright © 2013-2017 Thomas White <taw@bitwiz.org.uk>
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
#include "imagestore.h"


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
                      double min_y, double max_y)
{
	int i;
	double hpos = 0.0;

	cairo_save(cr);
	do_background(cr, fr);

	/* Actually render the contents */
	cairo_translate(cr, fr->pad_l, fr->pad_t);
	for ( i=0; i<fr->n_paras; i++ ) {

		double cur_h = paragraph_height(fr->paras[i]);

		cairo_save(cr);
		cairo_translate(cr, 0.0, hpos);

		if ( (hpos + cur_h > min_y) && (hpos < max_y) ) {
			render_paragraph(cr, fr->paras[i], is);
		} /* else paragraph is not visible */

		hpos += cur_h;
		cairo_restore(cr);

	}
	cairo_restore(cr);

	return 0;
}


int recursive_draw(struct frame *fr, cairo_t *cr,
                   ImageStore *is,
                   double min_y, double max_y)
{
	int i;

	draw_frame(cr, fr, is, min_y, max_y);

	for ( i=0; i<fr->num_children; i++ ) {
		cairo_save(cr);
		cairo_translate(cr, fr->children[i]->x, fr->children[i]->y);
		recursive_draw(fr->children[i], cr, is,
		               min_y - fr->children[i]->y,
		               max_y - fr->children[i]->y);
		cairo_restore(cr);
	}

	return 0;
}


void wrap_frame(struct frame *fr, PangoContext *pc)
{
	int i;
	double w;

	w = fr->w - fr->pad_l - fr->pad_r;

	for ( i=0; i<fr->n_paras; i++ ) {
		wrap_paragraph(fr->paras[i], pc, w, 0, 0);
	}
}


int recursive_wrap(struct frame *fr, PangoContext *pc)
{
	int i;

	wrap_frame(fr, pc);

	for ( i=0; i<fr->num_children; i++ ) {
		recursive_wrap(fr->children[i], pc);
	}

	return 0;
}


struct frame *interp_and_shape(SCBlock *scblocks, SCBlock **stylesheets,
                               SCCallbackList *cbl, ImageStore *is,
                               int slide_number,
                               PangoContext *pc, double w, double h,
                               PangoLanguage *lang)
{
	SCInterpreter *scin;
	char snum[64];
	struct frame *top;

	top = frame_new();
	top->resizable = 0;
	top->x = 0.0;
	top->y = 0.0;
	top->w = w;
	top->h = h;
	top->scblocks = scblocks;

	scin = sc_interp_new(pc, lang, is, top);
	if ( scin == NULL ) {
		fprintf(stderr, "Failed to set up interpreter.\n");
		frame_free(top);
		return NULL;
	}

	sc_interp_set_callbacks(scin, cbl);

	snprintf(snum, 63, "%i", slide_number);
	add_macro(scin, "slidenumber", snum);

	if ( stylesheets != NULL ) {
		int i = 0;
		while ( stylesheets[i] != NULL ) {
			sc_interp_run_stylesheet(scin, stylesheets[i]);
			i++;
		}
	}

	top->fontdesc = pango_font_description_copy(sc_interp_get_fontdesc(scin));
	top->col[0] = sc_interp_get_fgcol(scin)[0];
	top->col[1] = sc_interp_get_fgcol(scin)[1];
	top->col[2] = sc_interp_get_fgcol(scin)[2];
	top->col[3] = sc_interp_get_fgcol(scin)[3];

	sc_interp_add_blocks(scin, scblocks);

	sc_interp_destroy(scin);

	return top;
}


static struct frame *render_sc_with_context(SCBlock *scblocks,
                                 cairo_t *cr, double log_w, double log_h,
                                 SCBlock **stylesheets, SCCallbackList *cbl,
                                 ImageStore *is,
                                 int slide_number, PangoLanguage *lang,
				 PangoContext *pc)
{
	struct frame *top;

	cairo_rectangle(cr, 0.0, 0.0, log_w, log_h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	top = interp_and_shape(scblocks, stylesheets, cbl, is,
	                       slide_number, pc, log_w, log_h, lang);

	recursive_wrap(top, pc);

	recursive_draw(top, cr, is, 0.0, log_h);

	return top;
}


cairo_surface_t *render_sc(SCBlock *scblocks, int w, int h,
                           double log_w, double log_h,
                           SCBlock **stylesheets, SCCallbackList *cbl,
                           ImageStore *is,
                           int slide_number, struct frame **ptop,
                           PangoLanguage *lang)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	struct frame *top;
	PangoContext *pc;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(surf);
	pc = pango_cairo_create_context(cr);
	cairo_scale(cr, w/log_w, h/log_h);
	top = render_sc_with_context(scblocks, cr, log_w, log_h,
	                             stylesheets, cbl, is, slide_number,
	                             lang, pc);
	g_object_unref(pc);
	cairo_destroy(cr);

	*ptop = top;

	return surf;
}


static int safe_strcmp(const char *a, const char *b)
{
	if ( a == NULL ) return 1;
	if ( b == NULL ) return 1;
	return strcmp(a, b);
}


int export_pdf(struct presentation *p, const char *filename)
{
	double r;
	double w = 2048.0;
	double scale;
	cairo_surface_t *surf;
	cairo_t *cr;
	SCBlock *bl;
	int i;
	PangoContext *pc;

	r = p->slide_height / p->slide_width;

	surf = cairo_pdf_surface_create(filename, w, w*r);
	if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
		fprintf(stderr, "Couldn't create Cairo surface\n");
		return 1;
	}

	cr = cairo_create(surf);
	scale = w / p->slide_width;
	pc = pango_cairo_create_context(cr);

	i = 1;
	bl = p->scblocks;
	while ( bl != NULL ) {

		if ( safe_strcmp(sc_block_name(bl), "slide") != 0 ) {
			bl = sc_block_next(bl);
			continue;
		}

		SCBlock *stylesheets[2];

		stylesheets[0] = p->stylesheet;
		stylesheets[1] = NULL;

		cairo_save(cr);

		cairo_scale(cr, scale, scale);

		cairo_rectangle(cr, 0.0, 0.0, p->slide_width, p->slide_height);
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_fill(cr);

		render_sc_with_context(sc_block_child(bl), cr, p->slide_width,
		                     p->slide_height, stylesheets, NULL,
		                     p->is, i, p->lang, pc);

		cairo_restore(cr);

		cairo_show_page(cr);

		bl = sc_block_next(bl);
		i++;

	}

	g_object_unref(pc);
	cairo_surface_finish(surf);
	cairo_destroy(cr);

	return 0;
}
