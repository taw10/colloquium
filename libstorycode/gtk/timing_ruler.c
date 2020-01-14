/*
 * gtknarrativeview.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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
#include <gtk/gtk.h>
#include <assert.h>
#include <math.h>
#include <libintl.h>
#define _(x) gettext(x)

#include <narrative.h>

#include "narrative_priv.h"
#include "gtknarrativeview.h"


void draw_ruler(cairo_t *cr, GtkNarrativeView *e)
{
	int start_item, end_item;
	int i;
	double start_item_time = 0.0;
	double t;

	/* Background */
	cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, e->ruler_width, e->h);
	cairo_fill(cr);

	/* Which items are visible? */
	narrative_get_item_range(e->n, e->scroll_pos, e->scroll_pos+e->visible_height,
	                         &start_item, &end_item);

	for ( i=0; i<start_item; i++ ) {
		start_item_time += e->n->items[i].estd_duration;
	}
	t = start_item_time;
	for ( i=start_item; i<=end_item; i++ ) {
		//printf("%4i: %.2f mins\n", i, t);
		t += e->n->items[i].estd_duration;
	}
}
