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

#include "slide_render.h"
#include "presentation.h"


int render_slide(struct slide *s)
{
	cairo_surface_t *surf;
	cairo_t *cr;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                  s->slide_width, s->slide_height);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, s->slide_width, s->slide_height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_destroy(cr);

	if ( s->render_cache != NULL ) cairo_surface_destroy(s->render_cache);
	s->render_cache = surf;

	return 0;
}
