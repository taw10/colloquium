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

void narrative_free(Narrative *n)
{
	free(n->items);
	free(n);
}


static struct narrative_item *add_item(Narrative *n)
{
	struct narrative_item *new_items;
	new_items = realloc(n->items, (n->n_items+1)*sizeof(struct narrative_item));
	if ( new_items == NULL ) return NULL;
	n->items = new_items;
	return &n->items[n->n_items++];
}


void narrative_add_prestitle(Narrative *n, const char *text)
{
}


void narrative_add_bp(Narrative *n, const char *text)
{
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
}
