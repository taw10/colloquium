/*
 * frame.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
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

#include <assert.h>
#include <stdlib.h>

#include "frame.h"


static int alloc_ro(struct frame *fr)
{
	struct frame **new_ro;

	new_ro = realloc(fr->rendering_order,
	                 fr->max_ro*sizeof(struct frame *));
	if ( new_ro == NULL ) return 1;

	fr->rendering_order = new_ro;

	return 0;
}


struct frame *frame_new()
{
	struct frame *n;

	n = calloc(1, sizeof(struct frame));
	if ( n == NULL ) return NULL;

	n->rendering_order = NULL;
	n->max_ro = 32;
	alloc_ro(n);

	n->num_ro = 1;
	n->rendering_order[0] = n;

	n->pl = NULL;

	return n;
}


struct frame *add_subframe(struct frame *fr)
{
	struct frame *n;

	n = frame_new();
	if ( n == NULL ) return NULL;

	if ( fr->num_ro == fr->max_ro ) {
		fr->max_ro += 32;
		if ( alloc_ro(fr) ) return NULL;
	}

	fr->rendering_order[fr->num_ro++] = n;

	return n;
}


static void show_heirarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];

	strcpy(tn, t);
	strcat(tn, "  |-> ");

	printf("%s%p %s\n", t, fr, fr->sc);

	for ( i=0; i<fr->num_ro; i++ ) {
		if ( fr->rendering_order[i] != fr ) {
			show_heirarchy(fr->rendering_order[i], tn);
		} else {
			printf("%s<this frame>\n", tn);
		}
	}

}


static void recursive_unpack(struct frame *fr, const char *sc)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;

	bl = sc_find_blocks(sc, "f");

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		struct frame *sfr;
		sfr = add_subframe(fr);
		sfr->sc = remove_blocks(b->contents, "f");
		recursive_unpack(sfr, b->contents);
	}
	sc_block_list_free(bl);
}


/* Unpack level 2 StoryCode (content + subframes) into frames */
struct frame *sc_unpack(const char *sc)
{
	struct frame *fr;

	fr = frame_new();
	if ( fr == NULL ) return NULL;

	fr->sc = remove_blocks(sc, "f");
	recursive_unpack(fr, sc);

	show_heirarchy(fr, "");

	return fr;
}

