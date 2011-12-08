/*
 * slide_render.c
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
#include <pango/pangocairo.h>
#include <assert.h>

#include "slide_render.h"
#include "presentation.h"
#include "objects.h"
#include "stylesheet.h"


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


static cairo_surface_t *render_slide(struct slide *s, int w, int h)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;
	cairo_font_options_t *fopts;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);
	cairo_scale(cr, w/s->parent->slide_width, h/s->parent->slide_height);

	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_OFF);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_font_options(cr, fopts);

	for ( i=0; i<s->parent->ss->n_bgblocks; i++ ) {
		render_bgblock(cr, &s->parent->ss->bgblocks[i]);
	}

	for ( i=0; i<s->num_objects; i++ ) {

		struct object *o = s->objects[i];

		o->render_object(cr, o);

	}

	cairo_destroy(cr);
	cairo_font_options_destroy(fopts);

	return surf;
}


void redraw_slide(struct slide *s)
{
	int w, h;

	if ( s->rendered_thumb != NULL ) {
		cairo_surface_destroy(s->rendered_thumb);
	}

	w = s->parent->thumb_slide_width;
	h = (s->parent->slide_height/s->parent->slide_width) * w;
	s->rendered_thumb = render_slide(s, w, h);
	/* FIXME: Request redraw for slide sorter if open */

	/* Is this slide currently open in the editor? */
	if ( s == s->parent->cur_edit_slide ) {

		GtkWidget *da;

		if ( s->rendered_edit != NULL ) {
			cairo_surface_destroy(s->rendered_edit);
		}

		w = s->parent->edit_slide_width;
		h = (s->parent->slide_height/s->parent->slide_width) * w;
		s->rendered_edit = render_slide(s, w, h);

		da = s->parent->drawingarea;
		if ( da != NULL ) {
			gdk_window_invalidate_rect(da->window, NULL, FALSE);
		}

	}

	/* Is this slide currently being displayed on the projector? */
	if ( s == s->parent->cur_proj_slide ) {

		GtkWidget *da;

		if ( s->rendered_proj != NULL ) {
			cairo_surface_destroy(s->rendered_proj);
		}

		w = s->parent->proj_slide_width;
		h = (s->parent->slide_height/s->parent->slide_width) * w;
		s->rendered_proj = render_slide(s, w, h);

		da = s->parent->ss_drawingarea;
		if ( da != NULL ) {
			gdk_window_invalidate_rect(da->window, NULL, FALSE);
		}

	}
}


void draw_rubberband_box(cairo_t *cr, double x, double y,
                         double width, double height)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, width, height);
	cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);
}


void draw_resize_handle(cairo_t *cr, double x, double y)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, 20.0, 20.0);
	cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
	cairo_fill(cr);
}


void draw_editing_box(cairo_t *cr, double xmin, double ymin,
                      double width, double height)
{
	const double dash[] = {2.0, 2.0};

	cairo_new_path(cr);
	cairo_rectangle(cr, xmin-5.0, ymin-5.0, width+10.0, height+10.0);
	cairo_set_source_rgb(cr, 0.0, 0.69, 1.0);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);

	cairo_new_path(cr);
	cairo_rectangle(cr, xmin, ymin, width, height);
	cairo_set_dash(cr, dash, 2, 0.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.1);
	cairo_stroke(cr);

	cairo_set_dash(cr, NULL, 0, 0.0);
}
