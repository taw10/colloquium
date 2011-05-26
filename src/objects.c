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
	new->text = malloc(1);
	new->text[0] = '\0';
	new->text_len = 1;
	new->insertion_point = 0;

	s->object_seq++;

	return new;
}


void insert_text(struct object *o, char *t)
{
	char *tmp;
	size_t tlen, olen;
	int i;

	assert(o->type == TEXT);
	tlen = strlen(t);
	olen = strlen(o->text);

	if ( tlen + olen + 1 > o->text_len ) {

		char *try;

		try = realloc(o->text, o->text_len + tlen + 64);
		if ( try == NULL ) return;  /* Failed to insert */
		o->text = try;
		o->text_len += 64;
		o->text_len += tlen;

	}

	tmp = malloc(o->text_len);
	if ( tmp == NULL ) return;

	for ( i=0; i<o->insertion_point; i++ ) {
		tmp[i] = o->text[i];
	}
	for ( i=0; i<tlen; i++ ) {
		tmp[i+o->insertion_point] = t[i];
	}
	for ( i=0; i<olen-o->insertion_point; i++ ) {
		tmp[i+o->insertion_point+tlen] = o->text[i+o->insertion_point];
	}
	tmp[olen+tlen] = '\0';
	memcpy(o->text, tmp, o->text_len);
	free(tmp);

	o->insertion_point += tlen;
	o->parent->object_seq++;
	o->empty = 0;
}


void handle_text_backspace(struct object *o)
{
	size_t ndel;
	int i;

	assert(o->type == TEXT);

	if ( o->insertion_point == 0 ) return;  /* Nothing to delete */

	if ( !(o->text[o->insertion_point-1] & 0x80) ) {
		/* Simple (ASCII-style) backspace */
		ndel = 1;
	} else {
		ndel = 0;
		for ( i=1; i<=6; i++ ) {
			if ( !(o->text[o->insertion_point-i] & 0xC0) ) ndel++;
		}
	}

	memmove(o->text+o->insertion_point-ndel,
	        o->text+o->insertion_point,
	        o->text_len-o->insertion_point);

	o->insertion_point -= ndel;

	if ( strlen(o->text) == 0 ) o->empty = 1;

	o->parent->object_seq++;
}


void delete_object(struct object *o)
{
	remove_object_from_slide(o->parent, o);
	free_object(o);
}
