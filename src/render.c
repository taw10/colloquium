/*
 * render.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2011 Thomas White <taw@bitwiz.org.uk>
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

#include "storycode.h"
#include "stylesheet.h"
#include "presentation.h"


static void render_bgblock(cairo_t *cr, struct bgblock *b)
{
	GdkColor col1;
	GdkColor col2;
	cairo_pattern_t *patt = NULL;
	double cx, cy, r, r1, r2;

	cairo_rectangle(cr, b->min_x, b->min_y,
	                    b->max_x - b->min_x,
	                    b->max_y - b->min_y);

	switch ( b->type ) {

		case BGBLOCK_SOLID :
		gdk_color_parse(b->colour1, &col1);
		gdk_cairo_set_source_color(cr, &col1);
		/* FIXME: Honour alpha as well */
		cairo_fill(cr);
		break;

		case BGBLOCK_GRADIENT_CIRCULAR :
		cx = b->min_x + (b->max_x-b->min_x)/2.0;
		cy = b->min_y + (b->max_y-b->min_y)/2.0;
		r1 = (b->max_x-b->min_x)/2.0;
		r2 = (b->max_y-b->min_y)/2.0;
		r = r1 > r2 ? r1 : r2;
		patt = cairo_pattern_create_radial(cx, cy, r, cx, cy, 0.0);
		/* Fall-through */

		case BGBLOCK_GRADIENT_X :
		if ( patt == NULL ) {
			patt = cairo_pattern_create_linear(b->min_x, 0.0,
		                                           b->max_y, 0.0);
		}
		/* Fall-through */

		case BGBLOCK_GRADIENT_Y :
		if ( patt == NULL ) {
			patt = cairo_pattern_create_linear(0.0, b->min_y,
		                                           0.0, b->max_y);
		}

		gdk_color_parse(b->colour1, &col1);
		gdk_color_parse(b->colour2, &col2);
		cairo_pattern_add_color_stop_rgba(patt, 0.0, col1.red/65535.0,
		                                             col1.green/65535.0,
		                                             col1.blue/65535.0,
		                                             b->alpha1);
		cairo_pattern_add_color_stop_rgba(patt, 1.0, col2.red/65535.0,
		                                             col2.green/65535.0,
		                                             col2.blue/65535.0,
		                                             b->alpha2);
		cairo_set_source(cr, patt);
		cairo_fill(cr);
		cairo_pattern_destroy(patt);
		break;

		case BGBLOCK_IMAGE :
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_fill(cr);

	}
}


/* Render Level 1 Storycode */
int render_sc(const char *sc, cairo_t *cr, double w, double h, PangoContext *pc)
{
	PangoLayout *layout;
	PangoFontDescription *fontdesc;
	GdkColor col;

	/* FIXME: Check for Level 1 markup e.g. image */

	layout = pango_layout_new(pc);

	pango_cairo_update_layout(cr, layout);
	pango_layout_set_width(layout, w*PANGO_SCALE);
	pango_layout_set_height(layout, h*PANGO_SCALE);

	pango_layout_set_text(layout, sc, -1);
	fontdesc = pango_font_description_from_string("Sans 12");

	pango_layout_set_font_description(layout, fontdesc);

	cairo_move_to(cr, 0.0, 0.0);

	/* FIXME: Honour alpha as well */
	gdk_color_parse("#000000", &col);
	gdk_cairo_set_source_color(cr, &col);

	pango_cairo_show_layout(cr, layout);

	pango_font_description_free(fontdesc);
	g_object_unref(G_OBJECT(layout));

	return 0;
}


static int render_frame(struct frame *fr, cairo_t *cr, double w, double h)
{
	int i;

	/* The rendering order is a list of children, but it also contains the
	 * current frame.  In this way, it contains information about which
	 * layer the current frame is to be rendered as relative to its
	 * children. */
	for ( i=0; i<fr->num_ro; i++ ) {

		double nw, nh;

		if ( fr->rendering_order[i] == fr ) {
			render_sc(fr->sc, cr, w, h, fr->pc);
			continue;
		}

		/* Sort out the transformation for the margins */
		cairo_translate(cr, fr->rendering_order[i]->cl->margin_left,
		                    fr->rendering_order[i]->cl->margin_top);

		nw = w - fr->rendering_order[i]->cl->margin_left;
		nw    -= fr->rendering_order[i]->cl->margin_right;

		nh = h - fr->rendering_order[i]->cl->margin_top;
		nh    -= fr->rendering_order[i]->cl->margin_bottom;

		render_frame(fr->rendering_order[i], cr, nw, nh);

	}

	return 0;
}


cairo_surface_t *render_slide(struct slide *s, int w, int h)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	cairo_font_options_t *fopts;
	int i;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);
	cairo_scale(cr, w, h);

	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_OFF);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_font_options(cr, fopts);

	for ( i=0; i<s->st->n_bgblocks; i++ ) {
		render_bgblock(cr, s->st->bgblocks[i]);
	}

	render_frame(s->top, cr, w, h);

	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);

	return surf;
}
