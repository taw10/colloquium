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

	n->sc = NULL;

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


static LengthUnits get_units(const char *t)
{
	size_t len = strlen(t);

	if ( t[len-1] == 'f' ) return UNITS_FRAC;
	if ( t[len-1] == 'u' ) return UNITS_SLIDE;

	fprintf(stderr, "Invalid units in '%s'\n", t);
	return UNITS_SLIDE;
}


static void parse_option(struct frame *fr, const char *opt, StyleSheet *ss)
{
	if ( (index(opt, 'x') != NULL) && (index(opt, '+') != NULL)
	  && (index(opt, '+') != rindex(opt, '+')) )
	{
		char *w;
		char *h;
		char *x;
		char *y;
		char *check;

		/* Looks like a dimension/position thing */
		w = strdup(opt);
		h = index(w, 'x');
		h[0] = '\0';  h++;

		x = index(h, '+');
		if ( x == NULL ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		x[0] = '\0';  x++;

		y = index(x, '+');
		if ( x == NULL ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		y[0] = '\0';  y++;

		fr->lop.w = strtod(w, &check);
		if ( check == w ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		fr->lop.w_units = get_units(w);

		fr->lop.h = strtod(h, &check);
		if ( check == h ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		fr->lop.h_units = get_units(h);

		fr->lop.x = strtod(x, &check);
		if ( check == x ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		fr->lop.y = strtod(y, &check);
		if ( check == y ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}

	}

	if ( strncmp(opt, "style=", 6) == 0 ) {

		char *s;
		char *tmp;

		tmp = strdup(opt);

		if ( opt[strlen(opt)-1] == '*' ) {
			fr->lop_from_style = 1;
			tmp[strlen(tmp)-1] = '\0';
		} else {
			fr->lop_from_style = 0;
		}

		s = index(tmp, '=') + 1;
		fr->style = lookup_style(ss, s);
		free(tmp);
		if ( fr->style == NULL ) {
			fprintf(stderr, "Invalid style '%s'\n", opt);
			return;
		}
	}
}


static void parse_options(struct frame *fr, const char *opth, StyleSheet *ss)
{
	int i;
	size_t len;
	size_t start;
	char *opt;

	if ( opth == NULL ) return;

	opt = strdup(opth);

	len = strlen(opt);
	start = 0;

	for ( i=0; i<len; i++ ) {

		/* FIXME: comma might be escaped or quoted */
		if ( opt[i] == ',' ) {
			opt[i] = '\0';
			parse_option(fr, opt+start, ss);
			start = i+1;
		}

	}

	if ( start != len ) {
		parse_option(fr, opt+start, ss);
	}

	free(opt);
}


static int recursive_unpack(struct frame *fr, const char *sc, StyleSheet *ss)
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
		sfr->empty = 0;
		parse_options(sfr, b->options, ss);

		if ( sfr->lop.w < 0.0 ) {
			sfr->lop.x += sfr->lop.w;
			sfr->lop.w = -sfr->lop.w;
		}

		if ( sfr->lop.h < 0.0 ) {
			sfr->lop.y += sfr->lop.h;
			sfr->lop.h = -sfr->lop.h;
		}

		sfr->sc = remove_blocks(b->contents, "f");
		sfr->sc_len = strlen(sfr->sc)+1;
		if ( recursive_unpack(sfr, b->contents, ss) ) {
			sc_block_list_free(bl);
			return 1;
		}
	}
	sc_block_list_free(bl);

	return 0;
}


/* Unpack level 2 StoryCode (content + subframes) into frames */
struct frame *sc_unpack(const char *sc, StyleSheet *ss)
{
	struct frame *fr;

	fr = frame_new();
	if ( fr == NULL ) return NULL;

	fr->empty = 0;
	fr->sc = remove_blocks(sc, "f");
	if ( recursive_unpack(fr, sc, ss) ) {
		return NULL;
	}

	return fr;
}


void show_hierarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];
	char *sc;

	strcpy(tn, t);
	strcat(tn, "      ");

	sc = escape_text(fr->sc);
	printf("%s%p %s (%.2f x %.2f)\n", t, fr, sc, fr->w, fr->h);
	free(sc);

	for ( i=0; i<fr->num_children; i++ ) {
		show_hierarchy(fr->children[i], tn);
	}

}
