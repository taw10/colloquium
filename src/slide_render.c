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


int render_slide(struct slide *s)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                  s->parent->slide_width,
	                                  s->parent->slide_height);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0,
	                s->parent->slide_width, s->parent->slide_height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	for ( i=0; i<s->num_objects; i++ ) {

		struct object *o = s->objects[i];

		o->render_object(cr, o);

	}

	cairo_destroy(cr);

	if ( s->render_cache != NULL ) cairo_surface_destroy(s->render_cache);
	s->render_cache = surf;
	s->render_cache_seq = s->object_seq;

	return 0;
}


void check_redraw_slide(struct slide *s)
{
	/* Update necessary? */
	if ( s->object_seq <= s->render_cache_seq ) return;

	render_slide(s);
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
