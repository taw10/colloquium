/*
 * frame.c
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
 *
 * This file is part of Colloquium.
 *
 * Colloquium is free software: you can redistribute it and/or modify
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
#include <string.h>

#include "storycode.h"
#include "frame.h"


static int alloc_ro(struct frame *fr)
{
	struct frame **new_ro;

	new_ro = realloc(fr->children,
	                 fr->max_children*sizeof(struct frame *));
	if ( new_ro == NULL ) return 1;

	fr->children = new_ro;

	return 0;
}


struct frame *frame_new()
{
	struct frame *n;

	n = calloc(1, sizeof(struct frame));
	if ( n == NULL ) return NULL;

	n->children = NULL;
	n->max_children = 32;
	alloc_ro(n);

	n->num_children = 0;

	n->pl = NULL;
	n->contents = NULL;

	return n;
}


struct frame *add_subframe(struct frame *fr)
{
	struct frame *n;

	n = frame_new();
	if ( n == NULL ) return NULL;

	if ( fr->num_children == fr->max_children ) {
		fr->max_children += 32;
		if ( alloc_ro(fr) ) return NULL;
	}

	fr->children[fr->num_children++] = n;

	return n;
}


static int recursive_unpack(struct frame *fr, const char *sc)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;

	bl = sc_find_blocks(sc, "f");
	if ( bl == NULL ) return 1;

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		struct frame *sfr;
		sfr = add_subframe(fr);
		sfr->sc = remove_blocks(b->contents, "f");
		if ( recursive_unpack(sfr, b->contents) ) {
			sc_block_list_free(bl);
			return 1;
		}
	}
	sc_block_list_free(bl);

	return 0;
}


/* Unpack level 2 StoryCode (content + subframes) into frames */
struct frame *sc_unpack(const char *sc)
{
	struct frame *fr;

	fr = frame_new();
	if ( fr == NULL ) return NULL;

	fr->sc = remove_blocks(sc, "f");
	if ( recursive_unpack(fr, sc) ) {
		return NULL;
	}

	show_heirarchy(fr, "");

	return fr;
}


void show_heirarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];

	strcpy(tn, t);
	strcat(tn, "      ");

	printf("%s%p %s %p (%i x %i) / (%.2f x %.2f)\n", t, fr, fr->sc, fr->contents,
	       fr->pix_w, fr->pix_h, fr->w, fr->h);

	for ( i=0; i<fr->num_children; i++ ) {
		show_heirarchy(fr->children[i], tn);
	}

}
