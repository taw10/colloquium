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
#include "slide_priv.h"


Slide *slide_new()
{
	Slide *s;
	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;
	s->n_items = 0;
	s->items = NULL;
	s->logical_w = 1024.0;
	s->logical_h = 768.0;
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


static char units(enum length_unit u)
{
	if ( u == LENGTH_UNIT ) return 'u';
	if ( u == LENGTH_FRAC ) return 'f';
	return '?';
}


void describe_slide(Slide *s)
{
	int i;

	printf("  %i items\n", s->n_items);
	for ( i=0; i<s->n_items; i++ ) {
		printf("item %i: %i\n", i, s->items[i].type);
		printf("geom %f %c x %f %c + %f %c + %f %c\n",
	               s->items[i].geom.x.len, units(s->items[i].geom.x.unit),
	               s->items[i].geom.y.len, units(s->items[i].geom.y.unit),
	               s->items[i].geom.w.len, units(s->items[i].geom.w.unit),
	               s->items[i].geom.h.len, units(s->items[i].geom.h.unit));
	}
}


int slide_set_logical_size(Slide *s, double w, double h)
{
	if ( s == NULL ) return 1;
	s->logical_w = w;
	s->logical_h = h;
	return 0;
}


int slide_get_logical_size(Slide *s, double *w, double *h)
{
	if ( s == NULL ) return 1;
	*w = s->logical_w;
	*h = s->logical_h;
	return 0;
}
