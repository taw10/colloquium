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

struct _narrative
{
	int n_items;
	struct narrative_item *items;
};


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
