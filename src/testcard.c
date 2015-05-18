/*
 * testcard.c
 *
 * Copyright © 2013-2015 Thomas White <taw@bitwiz.org.uk>
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
#include <gdk/gdkkeysyms.h>

#include "inhibit_screensaver.h"
#include "presentation.h"


struct testcard
{
	GtkWidget *window;
	char geom[256];
	int slide_width;
	int slide_height;
	GtkWidget *drawingarea;
	struct inhibit_sys *inhibit;
	struct presentation *p;
};

static gint destroy_sig(GtkWidget *widget, struct testcard *tc)
{
	free(tc);
	return FALSE;
}


static void arrow_left(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, size, size);
	cairo_rel_line_to(cr, 0.0, -2*size);
	cairo_rel_line_to(cr, -size, size);
}


static void arrow_right(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, -size, size);
	cairo_rel_line_to(cr, 0.0, -2*size);
	cairo_rel_line_to(cr, size, size);
}


static void arrow_down(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, -size, -size);
	cairo_rel_line_to(cr, 2*size, 0.0);
	cairo_rel_line_to(cr, -size, size);
}


static void arrow_up(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, -size, size);
	cairo_rel_line_to(cr, 2*size, 0.0);
	cairo_rel_line_to(cr, -size, -size);
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, struct testcard *tc)
{
	double xoff, yoff;
	double width, height;
	int h;
	PangoLayout *pl;
	PangoFontDescription *desc;
	char tmp[1024];
	int plw, plh;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_fill(cr);

	/* FIXME: Assumes that monitor and slide sizes are such that
	 * letterboxing at sides.  This needn't be the case. */
	h = tc->slide_width * tc->p->slide_height / tc->p->slide_width;

	/* Get the overall size */
	xoff = (width - tc->slide_width)/2.0;
	yoff = (height - h)/2.0;

	/* Background of slide */
	cairo_rectangle(cr, xoff, yoff, tc->slide_width, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	snprintf(tmp, 1024, "Colloquium "PACKAGE_VERSION" test card\n"
	                    "Screen resolution %.0f × %.0f\n"
	                    "Slide resolution %i × %i", width, height,
	                    tc->slide_width, h);

	pl = pango_cairo_create_layout(cr);
	desc = pango_font_description_from_string("Sans 24");
	pango_layout_set_font_description(pl, desc);
	pango_layout_set_text(pl, tmp, -1);

	pango_layout_get_size(pl, &plw, &plh);
	plw = pango_units_to_double(plw);
	plh = pango_units_to_double(plh);
	cairo_move_to(cr, (width-plw)/2, (height-plh)/2);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	pango_cairo_show_layout(cr, pl);

	/* Arrows showing edges of screen */
	cairo_move_to(cr, 0.0, height/2);
	arrow_left(cr, 100.0);
	cairo_fill(cr);
	cairo_move_to(cr, width, height/2);
	arrow_right(cr, 100.0);
	cairo_fill(cr);
	cairo_move_to(cr, width/2, height);
	arrow_down(cr, 100.0);
	cairo_fill(cr);
	cairo_move_to(cr, width/2, 0.0);
	arrow_up(cr, 100.0);
	cairo_fill(cr);

	/* Arrows showing edges of slide */
	cairo_translate(cr, xoff, yoff);
	cairo_set_source_rgb(cr, 0.5, 0.0, 0.0);
	cairo_move_to(cr, 0.0, 100+h/2);
	arrow_left(cr, 80.0);
	cairo_fill(cr);
	cairo_move_to(cr, tc->slide_width, 100+h/2);
	arrow_right(cr, 80.0);
	cairo_fill(cr);
	cairo_move_to(cr, 100+tc->slide_width/2, h);
	arrow_down(cr, 80.0);
	cairo_fill(cr);
	cairo_move_to(cr, 100+tc->slide_width/2, 0.0);
	arrow_up(cr, 80.0);
	cairo_fill(cr);
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              struct testcard *tc)
{
	gtk_widget_destroy(tc->window);
	return FALSE;
}


static gboolean realize_sig(GtkWidget *w, struct testcard *tc)
{
	gtk_window_parse_geometry(GTK_WINDOW(w), tc->geom);
	return FALSE;
}


void show_testcard(struct presentation *p)
{
	GdkScreen *screen;
	int n_monitors;
	int i;
	struct testcard *tc;
	double slide_width = 1024.0; /* Logical slide size */
	double slide_height = 768.0; /* FIXME: Should come from slide */

	tc = calloc(1, sizeof(struct testcard));
	if ( tc == NULL ) return;

	tc->p = p;

	if ( tc->inhibit == NULL ) {
		tc->inhibit = inhibit_prepare();
	}

	tc->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	tc->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(tc->window), tc->drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(tc->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(tc->drawingarea), GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(tc->drawingarea), "key-press-event",
			 G_CALLBACK(key_press_sig), tc);
	g_signal_connect(G_OBJECT(tc->window), "destroy",
	                 G_CALLBACK(destroy_sig), tc);
	g_signal_connect(G_OBJECT(tc->window), "realize",
	                 G_CALLBACK(realize_sig), tc);
	g_signal_connect(G_OBJECT(tc->drawingarea), "draw",
			 G_CALLBACK(draw_sig), tc);

	gtk_widget_grab_focus(GTK_WIDGET(tc->drawingarea));

	screen = gdk_screen_get_default();
	n_monitors = gdk_screen_get_n_monitors(screen);
	for ( i=0; i<n_monitors; i++ ) {

		GdkRectangle rect;
		int w;

		gdk_screen_get_monitor_geometry(screen, i, &rect);
		snprintf(tc->geom, 255, "%ix%i+%i+%i",
		         rect.width, rect.height, rect.x, rect.y);

		w = rect.height * slide_width/slide_height;
		if ( w > rect.width ) w = rect.width;
		tc->slide_width = w;
		tc->slide_height = rect.height;

	} /* FIXME: Sensible (configurable) choice of monitor */

	gtk_window_fullscreen(GTK_WINDOW(tc->window));
	gtk_widget_show_all(GTK_WIDGET(tc->window));

	if ( tc->inhibit != NULL ) do_inhibit(tc->inhibit, 1);
}

