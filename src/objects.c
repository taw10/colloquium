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

#include "presentation.h"
#include "objects.h"


static struct object *new_object(enum objtype t)
{
	struct object *new;

	new = malloc(sizeof(struct object));
	if ( new == NULL ) return NULL;

	new->type = t;
	new->empty = 1;
	new->parent = NULL;

	return new;
}


static void free_object(struct object *o)
{
	free(o);
}


struct object *add_text_object(struct slide *s, double x, double y)
{
	struct object *new;

	new = new_object(TEXT);
	if ( add_object_to_slide(s, new) ) {
		free_object(new);
		return NULL;
	}

	new->x = x;  new->y = y;
	new->bb_width = 10.0;
	new->bb_height = 40.0;
	new->text = "Hello";

	s->object_seq++;

	return new;
}


void delete_object(struct object *o)
{
	remove_object_from_slide(o->parent, o);
	free_object(o);
}
