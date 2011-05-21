/*
 * presentation.c
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

#include <stdlib.h>
#include <string.h>

#include "presentation.h"
#include "slide_render.h"


int add_slide(struct presentation *p)
{
	struct slide **try;
	struct slide *new;

	try = realloc(p->slides, p->num_slides*sizeof(struct slide **));
	if ( try == NULL ) return 1;
	p->slides = try;

	new = malloc(sizeof(struct slide));
	if ( new == NULL ) return 1;
	/* Doesn't matter that p->slides now has some excess space -
	 * it'll get corrected the next time a slide is added or deleted. */

	/* No objects to start with */
	new->n_objects = 0;
	new->object_seq = 0;
	new->objects = NULL;

	new->slide_width = p->slide_width;
	new->slide_height = p->slide_height;

	new->render_cache_seq = 0;
	new->render_cache = NULL;
	render_slide(new);  /* Render nothing, just to make the surface exist */

	p->slides[p->num_slides++] = new;
	return 0;
}


struct presentation *new_presentation()
{
	struct presentation *new;

	new = malloc(sizeof(struct presentation));

	new->titlebar = strdup("(untitled)");
	new->filename = NULL;

	new->window = NULL;
	new->ui = NULL;
	new->action_group = NULL;

	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	new->num_slides = 0;
	new->slides = NULL;
	add_slide(new);

	new->view_slide = 0;

	return new;
}
