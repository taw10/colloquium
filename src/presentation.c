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
#include <assert.h>

#include "presentation.h"
#include "slide_render.h"
#include "objects.h"


struct slide *add_slide(struct presentation *p)
{
	struct slide **try;
	struct slide *new;

	try = realloc(p->slides, (1+p->num_slides)*sizeof(struct slide *));
	if ( try == NULL ) return NULL;
	p->slides = try;

	new = malloc(sizeof(struct slide));
	if ( new == NULL ) return NULL;
	/* Doesn't matter that p->slides now has some excess space -
	 * it'll get corrected the next time a slide is added or deleted. */

	/* No objects to start with */
	new->num_objects = 0;
	new->object_seq = 0;
	new->objects = NULL;

	new->slide_width = p->slide_width;
	new->slide_height = p->slide_height;

	new->render_cache_seq = 0;
	new->render_cache = NULL;
	render_slide(new);  /* Render nothing, just to make the surface exist */

	p->slides[p->num_slides++] = new;
	printf("Now %i slides\n", p->num_slides);
	return new;
}


int add_object_to_slide(struct slide *s, struct object *o)
{
	struct object **try;

	try = realloc(s->objects, (1+s->num_objects)*sizeof(struct object *));
	if ( try == NULL ) return 1;
	s->objects = try;

	s->objects[s->num_objects++] = o;
	o->parent = s;

	printf("Now %i objects in slide %p\n", s->num_objects, s);

	return 0;
}


void remove_object_from_slide(struct slide *s, struct object *o)
{
	int i;
	int found = 0;

	for ( i=0; i<s->num_objects; i++ ) {

		if ( s->objects[i] == o ) {
			assert(!found);
			found = 1;
			continue;
		}

		if ( found ) {
			if ( i == s->num_objects-1 ) {
				s->objects[i] = NULL;
			} else {
				s->objects[i] = s->objects[i+1];
			}
		}

	}

	s->num_objects--;
}


struct object *find_object_at_position(struct slide *s, double x, double y)
{
	int i;
	struct object *o = NULL;

	for ( i=0; i<s->num_objects; i++ ) {

		if ( (x>s->objects[i]->x) && (y>s->objects[i]->y)
		  && (x<s->objects[i]->x+s->objects[i]->bb_width)
		  && (y<s->objects[i]->y+s->objects[i]->bb_height) )
		{
			o = s->objects[i];
		}

	}

	return o;
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

	/* Add one blank slide and view it */
	new->num_slides = 0;
	new->slides = NULL;
	new -> view_slide = add_slide(new);
	new->view_slide_number = 0;

	new->editing_object = NULL;

	return new;
}
