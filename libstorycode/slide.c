/*
 * slide.c
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "slide.h"

enum slide_item_type
{
	SLIDE_ITEM_TEXT,
	SLIDE_ITEM_IMAGE,
	SLIDE_ITEM_FOOTER,
	SLIDE_ITEM_SLIDETITLE,
	SLIDE_ITEM_PRESTITLE,
};


struct slide_item
{
	enum slide_item_type type;

	/* For TEXT */
	char **paragraphs;
	int n_paras;

	/* For IMAGE */
	char *filename;

	/* For SLIDETITLE */
	char *text;

	/* For TEXT and IMAGE */
	struct frame_geom geom;
};


struct _slide
{
	int n_items;
	struct slide_item *items;
};


Slide *slide_new()
{
	Slide *s;
	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;
	s->n_items = 0;
	s->items = NULL;
	return s;
}

void slide_free(Slide *s)
{
	free(s->items);
	free(s);
}


static struct slide_item *add_item(Slide *s)
{
	struct slide_item *new_items;
	new_items = realloc(s->items, (s->n_items+1)*sizeof(struct slide_item));
	if ( new_items == NULL ) return NULL;
	s->items = new_items;
	return &s->items[s->n_items++];
}


int slide_add_prestitle(Slide *s, char *prestitle)
{
	struct slide_item *item;

	item = add_item(s);
	if ( item == NULL ) return 1;

	item->type = SLIDE_ITEM_PRESTITLE;
	return 0;
}


int slide_add_image(Slide *s, char *filename, struct frame_geom geom)
{
	struct slide_item *item;

	item = add_item(s);
	if ( item == NULL ) return 1;

	item->type = SLIDE_ITEM_IMAGE;
	item->geom = geom;
	item->filename = filename;

	return 0;
}


int slide_add_text(Slide *s, char **text, int n_text, struct frame_geom geom)
{
	int i;
	struct slide_item *item;

	item = add_item(s);
	if ( item == NULL ) return 1;

	item->type = SLIDE_ITEM_TEXT;
	item->paragraphs = malloc(n_text*sizeof(char *));
	if ( item->paragraphs == NULL ) {
		s->n_items--;
		return 1;
	}

	for ( i=0; i<n_text; i++ ) {
		item->n_paras = n_text;
		item->paragraphs[i] = text[i];
	}
	item->n_paras = n_text;

	item->geom = geom;

	return 0;
}


int slide_add_footer(Slide *s)
{
	struct slide_item *item;

	item = add_item(s);
	if ( item == NULL ) return 1;

	item->type = SLIDE_ITEM_FOOTER;

	return 0;
}


int slide_add_slidetitle(Slide *s, char *slidetitle)
{
	struct slide_item *item;

	item = add_item(s);
	if ( item == NULL ) return 1;

	item->type = SLIDE_ITEM_SLIDETITLE;
	item->text = slidetitle;

	return 0;
}


void describe_slide(Slide *s)
{
	int i;

	printf("  %i items\n", s->n_items);
	for ( i=0; i<s->n_items; i++ ) {
		printf("item %i: %i\n", i, s->items[i].type);
	}
}
