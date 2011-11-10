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


static cairo_surface_t *render_slide(struct slide *s, int w, int h)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_scale(cr, w/s->parent->slide_width, h/s->parent->slide_height);

	for ( i=0; i<s->num_objects; i++ ) {

		struct object *o = s->objects[i];

		o->render_object(cr, o);

	}

	cairo_destroy(cr);

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
