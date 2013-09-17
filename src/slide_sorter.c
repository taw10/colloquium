/*
 * slide_sorter.c
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>

#include "presentation.h"


struct slide_sorter
{
	GtkWidget *window;
	GtkWidget *da;
};


static gint close_slidesorter_sig(GtkWidget *w, struct presentation *p)
{
	p->slide_sorter = NULL;
	return FALSE;
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, struct slide_sorter *n)
{
	int width, height;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 00.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	return FALSE;
}


void open_slidesorter(struct presentation *p)
{
	struct slide_sorter *n;

	if ( p->slide_sorter != NULL ) return;  /* Already open */

	n = malloc(sizeof(struct slide_sorter));
	if ( n == NULL ) return;
	p->slide_sorter = n;

	n->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(n->window), 500, 500);

	gtk_window_set_title(GTK_WINDOW(n->window), "Slide sorter");

	n->da = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(n->window), n->da);

	g_signal_connect(G_OBJECT(n->da), "draw", G_CALLBACK(draw_sig), n);
	g_signal_connect(G_OBJECT(n->window), "destroy",
	                 G_CALLBACK(close_slidesorter_sig), p);


	gtk_widget_show_all(n->window);
}
