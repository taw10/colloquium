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

	int width;  /* Number of slides across */
	struct presentation *p;
};


static gint close_slidesorter_sig(GtkWidget *w, struct presentation *p)
{
	p->slide_sorter = NULL;
	return FALSE;
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, struct slide_sorter *n)
{
	int width, height;
	int tw, th;
	int i;
	const int bw = 5.0;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	tw = n->p->thumb_slide_width;
	th = (n->p->slide_height/n->p->slide_width) * n->p->thumb_slide_width;

	for ( i=0; i<n->p->num_slides; i++ ) {

		int x = i % n->width;
		int y = i / n->width;

		cairo_save(cr);

		cairo_translate(cr, x*(tw+2*bw), y*(th+2*bw));
		cairo_translate(cr, bw, bw);  /* Border */
		cairo_rectangle(cr, 0.0, 0.0, tw, th);
		cairo_set_source_surface(cr, n->p->slides[i]->rendered_thumb,
		                         0.0, 0.0);
		cairo_fill(cr);

		cairo_rectangle(cr, 0.5, 0.5, tw, th);
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);

		cairo_restore(cr);

	}

	return FALSE;
}


static void size_sig(GtkWidget *da, GdkRectangle *size, struct slide_sorter *n)
{
	int w, h;
	int tw, th;
	const int bw = 5.0;

	tw = n->p->thumb_slide_width;
	th = (n->p->slide_height/n->p->slide_width) * n->p->thumb_slide_width;

	w = size->width;
	n->width = w / (tw+2*bw);

	h = (n->p->num_slides / n->width) * (th+2*bw);

	gtk_widget_set_size_request(da, w, h);  /* FIXME: Doesn't work */
}


void open_slidesorter(struct presentation *p)
{
	struct slide_sorter *n;
	GtkWidget *sw;

	if ( p->slide_sorter != NULL ) return;  /* Already open */

	n = malloc(sizeof(struct slide_sorter));
	if ( n == NULL ) return;
	p->slide_sorter = n;

	n->width = 6;
	n->p = p;

	n->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(n->window), 500, 500);

	n->da = gtk_drawing_area_new();

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
	                               GTK_POLICY_NEVER,
	                               GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), n->da);
	gtk_container_add(GTK_CONTAINER(n->window), sw);

	gtk_window_set_title(GTK_WINDOW(n->window), "Slide sorter");

	g_signal_connect(G_OBJECT(n->da), "draw", G_CALLBACK(draw_sig), n);
	g_signal_connect(G_OBJECT(n->da), "size-allocate", G_CALLBACK(size_sig),
	                 n);
	g_signal_connect(G_OBJECT(n->window), "destroy",
	                 G_CALLBACK(close_slidesorter_sig), p);

	gtk_widget_show_all(n->window);
}
