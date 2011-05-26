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

#include "slide_render.h"
#include "presentation.h"
#include "objects.h"


static void render_text_object(cairo_t *cr, struct object *o)
{
	PangoLayout *l;
	PangoFontDescription *d;
	int width, height;

	l = pango_cairo_create_layout(cr);
	pango_layout_set_text(l, o->text, -1);
	d = pango_font_description_from_string("Sans 30");
	pango_layout_set_font_description(l, d);
	pango_font_description_free(d);

	pango_cairo_update_layout(cr, l);
	pango_layout_get_size(l, &width, &height);

	o->bb_width = width/PANGO_SCALE;
	o->bb_height = height/PANGO_SCALE;

	cairo_move_to(cr, o->x, o->y);

	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	pango_cairo_show_layout(cr, l);
}


int render_slide(struct slide *s)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                  s->slide_width, s->slide_height);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, s->slide_width, s->slide_height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	for ( i=0; i<s->num_objects; i++ ) {

		struct object *o = s->objects[i];

		switch ( o->type ) {
		case TEXT :
			render_text_object(cr, o);
			break;
		}

	}

	cairo_destroy(cr);

	if ( s->render_cache != NULL ) cairo_surface_destroy(s->render_cache);
	s->render_cache = surf;
	s->render_cache_seq = s->object_seq;

	return 0;
}
