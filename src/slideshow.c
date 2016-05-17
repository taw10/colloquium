/*
 * slideshow.c
 *
 * Copyright © 2013-2016 Thomas White <taw@bitwiz.org.uk>
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

#include "presentation.h"
#include "render.h"
#include "pr_clock.h"
#include "inhibit_screensaver.h"
#include "frame.h"

G_DEFINE_TYPE_WITH_CODE(SCSlideshow, sc_slideshow, GTK_TYPE_WINDOW, NULL)


static void sc_slideshow_init(SCSlideshow *ss)
{
}


void sc_slideshow_class_init(SCSlideshowClass *klass)
{
}


static void slideshow_rerender(SCSlideshow *ss)
{
	int n;
	SCBlock *stylesheets[2];
	gint w, h;

	if ( ss->cur_slide == NULL ) return;

	if ( ss->surface != NULL ) {
		cairo_surface_destroy(ss->surface);
	}

	stylesheets[0] = ss->p->stylesheet;
	stylesheets[1] = NULL;

	n = slide_number(ss->p, ss->cur_slide);
	ss->surface = render_sc(sc_block_child(ss->cur_slide),
	                        ss->slide_width, ss->slide_height,
	                        ss->p->slide_width, ss->p->slide_height,
	                        stylesheets, NULL, ss->p->is, ISZ_SLIDESHOW, n,
	                        &ss->top, ss->p->lang);

	w = gtk_widget_get_allocated_width(GTK_WIDGET(ss->drawingarea));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(ss->drawingarea));

	gtk_widget_queue_draw_area(ss->drawingarea, 0, 0, w, h);
}


static gint ss_destroy_sig(GtkWidget *widget, SCSlideshow *ss)
{
	g_object_unref(ss->blank_cursor);
	if ( ss->surface != NULL ) {
		cairo_surface_destroy(ss->surface);
	}
	return FALSE;
}


static gboolean ss_draw_sig(GtkWidget *da, cairo_t *cr, SCSlideshow *ss)
{
	double xoff, yoff;
	double width, height;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_fill(cr);

	if ( !ss->blank ) {

		int h;

		/* FIXME: Assumes that monitor and slide sizes are such that
		 * letterboxing at sides.  This needn't be the case. */
		h = ss->slide_width * ss->p->slide_height / ss->p->slide_width;

		/* Get the overall size */
		xoff = (width - ss->slide_width)/2.0;
		yoff = (height - h)/2.0;

		/* Draw the slide from the cache */
		cairo_rectangle(cr, xoff, yoff, ss->slide_width, h);
		cairo_set_source_surface(cr, ss->surface, xoff, yoff);
		cairo_fill(cr);

	}

	return FALSE;
}


static gboolean ss_realize_sig(GtkWidget *w, SCSlideshow *ss)
{
	GdkWindow *win;

	win = gtk_widget_get_window(w);

	ss->blank_cursor = gdk_cursor_new_for_display(gdk_display_get_default(),
	                                              GDK_BLANK_CURSOR);
	gdk_window_set_cursor(GDK_WINDOW(win), ss->blank_cursor);

	gtk_window_parse_geometry(GTK_WINDOW(w), ss->geom);
	slideshow_rerender(ss);

	return FALSE;
}


void sc_slideshow_set_slide(SCSlideshow *ss, SCBlock *ns)
{
	ss->cur_slide = ns;
	slideshow_rerender(ss);
}


SCSlideshow *sc_slideshow_new(struct presentation *p)
{
	GdkScreen *screen;
	int n_monitors;
	int i;
	SCSlideshow *ss;
	double slide_width = 1024.0; /* Logical slide size */
	double slide_height = 768.0; /* FIXME: Should come from slide */

	ss = g_object_new(SC_TYPE_SLIDESHOW, NULL);
	if ( ss == NULL ) return NULL;

	ss->blank = 0;
	ss->p = p;
	ss->cur_slide = NULL;

	if ( ss->inhibit == NULL ) {
		ss->inhibit = inhibit_prepare();
	}

	ss->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(ss), ss->drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(ss->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(ss->drawingarea),
	                      GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(ss), "destroy",
	                 G_CALLBACK(ss_destroy_sig), ss);
	g_signal_connect(G_OBJECT(ss), "realize",
	                 G_CALLBACK(ss_realize_sig), ss);

	g_signal_connect(G_OBJECT(ss->drawingarea), "draw",
			 G_CALLBACK(ss_draw_sig), ss);

	gtk_widget_grab_focus(GTK_WIDGET(ss->drawingarea));

	screen = gdk_screen_get_default();
	n_monitors = gdk_screen_get_n_monitors(screen);
	for ( i=0; i<n_monitors; i++ ) {

		GdkRectangle rect;
		int w;

		gdk_screen_get_monitor_geometry(screen, i, &rect);
		snprintf(ss->geom, 255, "%ix%i+%i+%i",
		         rect.width, rect.height, rect.x, rect.y);

		w = rect.height * slide_width/slide_height;
		if ( w > rect.width ) w = rect.width;
		ss->slide_width = w;
		ss->slide_height = rect.height;

	} /* FIXME: Sensible (configurable) choice of monitor */

	ss->linked = 1;
	gtk_window_fullscreen(GTK_WINDOW(ss));
	gtk_widget_show_all(GTK_WIDGET(ss));

	if ( ss->inhibit != NULL ) do_inhibit(ss->inhibit, 1);

	return ss;
}

