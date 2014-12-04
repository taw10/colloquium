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

#include "sc_parse.h"
#include "frame.h"
#include "wrap.h"


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
	if ( alloc_ro(n) ) {
		fprintf(stderr, "Couldn't allocate children\n");
		free(n);
		return NULL;
	}
	n->num_children = 0;

	n->scblocks = NULL;

	n->boxes = malloc(sizeof(struct wrap_line));
	if ( n->boxes == NULL ) {
		fprintf(stderr, "Failed to allocate boxes.\n");
		free(n);
		return NULL;
	}
	initialise_line(n->boxes);

	return n;
}


void renew_frame(struct frame *fr)
{
	int i;

	if ( fr == NULL ) return;

	for ( i=0; i<fr->n_lines; i++ ) {
		wrap_line_free(&fr->lines[i]);
	}
	free(fr->lines);
	fr->lines = NULL;
	fr->n_lines = 0;
	fr->max_lines = 0;

	if ( fr->boxes != NULL ) {
		free(fr->boxes->boxes);
		free(fr->boxes);
	}
	fr->boxes = malloc(sizeof(struct wrap_line));
	initialise_line(fr->boxes);
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


void show_hierarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];

	strcpy(tn, t);
	strcat(tn, "      ");

	printf("%s%p (%.2f x %.2f)\n", t, fr, fr->w, fr->h);

	for ( i=0; i<fr->num_children; i++ ) {
		show_hierarchy(fr->children[i], tn);
	}

}
