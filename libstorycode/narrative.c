/*
 * narrative.c
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

#include "narrative.h"
#include "narrative_priv.h"

Narrative *narrative_new()
{
	Narrative *n;
	n = malloc(sizeof(*n));
	if ( n == NULL ) return NULL;
	n->n_items = 0;
	n->items = NULL;
	return n;
}


static void narrative_item_destroy(struct narrative_item *item)
{
	free(item->text);
#ifdef HAVE_PANGO
	if ( item->layout != NULL ) {
		g_object_unref(item->layout);
	}
#endif
#ifdef HAVE_CAIRO
	if ( item->slide_thumbnail != NULL ) {
		cairo_surface_destroy(item->slide_thumbnail);
	}
#endif
}


void narrative_free(Narrative *n)
{
	int i;
	for ( i=0; i<n->n_items; i++ ) {
		narrative_item_destroy(&n->items[i]);
	}
	free(n->items);
	free(n);
}


static struct narrative_item *add_item(Narrative *n)
{
	struct narrative_item *new_items;
	struct narrative_item *item;
	new_items = realloc(n->items, (n->n_items+1)*sizeof(struct narrative_item));
	if ( new_items == NULL ) return NULL;
	n->items = new_items;
	item = &n->items[n->n_items++];
	item->layout = NULL;
	item->text = NULL;
	item->slide = NULL;
	item->slide_thumbnail = NULL;
	return item;
}


void narrative_add_prestitle(Narrative *n, char *text)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_PRESTITLE;
	item->text = text;
	item->align = ALIGN_LEFT;
	item->layout = NULL;
}


void narrative_add_bp(Narrative *n, char *text)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_BP;
	item->text = text;
	item->align = ALIGN_LEFT;
	item->layout = NULL;
}


void narrative_add_text(Narrative *n, char *text)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_TEXT;
	item->text = text;
	item->align = ALIGN_LEFT;
	item->layout = NULL;
}


void narrative_add_slide(Narrative *n, Slide *slide)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_SLIDE;
	item->slide = slide;
	item->slide_thumbnail = NULL;
}


static void delete_item(Narrative *n, int del)
{
	int i;
	narrative_item_destroy(&n->items[del]);
	for ( i=del; i<n->n_items-1; i++ ) {
		n->items[i] = n->items[i+1];
	}
	n->n_items--;
}


/* Delete from item i1 offset o1 to item i2 offset o2, inclusive */
void narrative_delete_block(Narrative *n, int i1, size_t o1, int i2, size_t o2)
{
	int i;

	/* Starting item */
	if ( n->items[i1].type == NARRATIVE_ITEM_SLIDE ) {
		delete_item(n, i1);
		i1--;
		i2--;
	} else {
		if ( i1 == i2 ) {
			memmove(&n->items[i1].text[o1],
			        &n->items[i1].text[o2],
			        strlen(n->items[i1].text)-o2+1);
			return;  /* easy case */
		} else {
			n->items[i1].text[o1] = '\0';
		}
	}

	/* Middle items */
	for ( i=i1+1; i<i2; i++ ) {
		delete_item(n, i);
		i2--;
	}

	/* Last item */
	if ( n->items[i2].type == NARRATIVE_ITEM_SLIDE ) {
		delete_item(n, i2);
	} else {
		memmove(&n->items[i2].text[0],
		        &n->items[i2].text[o2], o2);
	}
}
