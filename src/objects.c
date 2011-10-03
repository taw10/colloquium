/*
 * objects.c
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
#include "objects.h"
#include "mainwindow.h"


struct object *add_image_object(struct slide *s, double x, double y,
                                const char *filename,
                                double width, double height)
{
	struct object *new;

	new = NULL; /* FIXME! Generally */

	new->x = x;  new->y = y;
	new->bb_width = width;
	new->bb_height = height;

	if ( add_object_to_slide(s, new) ) {
		delete_object(new);
		return NULL;
	}
	s->object_seq++;

	return new;
}


void notify_style_update(struct presentation *p, struct style *sty)
{
	int i;
	int changed = 0;

	for ( i=0; i<p->num_slides; i++ ) {

		int j;
		struct slide *s;

		s = p->slides[i];

		for ( j=0; j<p->slides[i]->num_objects; j++ ) {

			if ( s->objects[j]->style != sty ) continue;

			s->object_seq++;
			if ( p->view_slide == s ) changed = 1;
			break;

		}

	}

	p->completely_empty = 0;
	if ( changed ) notify_slide_update(p);
}


void delete_object(struct object *o)
{
	if ( o->parent != NULL ) remove_object_from_slide(o->parent, o);
	o->delete_object(o);
	free(o);
}
