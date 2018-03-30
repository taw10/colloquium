/*
 * slideshow.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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

#include "colloquium.h"
#include "presentation.h"
#include "render.h"
#include "pr_clock.h"
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
	ss->surface = render_sc(ss->cur_slide,
	                        ss->slide_width, ss->slide_height,
	                        ss->p->slide_width, ss->p->slide_height,
	                        stylesheets, NULL, ss->p->is, n,
	                        &ss->top, ss->p->lang);

	w = gtk_widget_get_allocated_width(GTK_WIDGET(ss->drawingarea));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(ss->drawingarea));

	gtk_widget_queue_draw_area(ss->drawingarea, 0, 0, w, h);
}


static gint ssh_destroy_sig(GtkWidget *widget, SCSlideshow *ss)
{
	if ( ss->blank_cursor != NULL ) {
		g_object_unref(ss->blank_cursor);
	}
	if ( ss->surface != NULL ) {
		cairo_surface_destroy(ss->surface);
	}
	if ( ss->inhibit_cookie ) {
		gtk_application_uninhibit(ss->app, ss->inhibit_cookie);
	}
	return FALSE;
}


static gboolean ss_draw_sig(GtkWidget *da, cairo_t *cr, SCSlideshow *ss)
{
	double width, height;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_fill(cr);

	if ( !ss->blank ) {

		/* Draw the slide from the cache */
		cairo_rectangle(cr, ss->xoff, ss->yoff,
		                ss->slide_width, ss->slide_height);
		cairo_set_source_surface(cr, ss->surface, ss->xoff, ss->yoff);
		cairo_fill(cr);

	}

	return FALSE;
}


static gboolean ss_realize_sig(GtkWidget *w, SCSlideshow *ss)
{
	if ( (ss->app == NULL) || colloquium_get_hidepointer(COLLOQUIUM(ss->app)) ) {

		/* Hide the pointer */
		GdkWindow *win;
		win = gtk_widget_get_window(w);
		ss->blank_cursor = gdk_cursor_new_for_display(gdk_display_get_default(),
		                                              GDK_BLANK_CURSOR);
		gdk_window_set_cursor(GDK_WINDOW(win), ss->blank_cursor);

	} else {
		ss->blank_cursor = NULL;
	}

	slideshow_rerender(ss);

	return FALSE;
}


static void ss_size_sig(GtkWidget *widget, GdkRectangle *rect, SCSlideshow *ss)
{
	const double sw = ss->p->slide_width;
	const double sh = ss->p->slide_height;

	if ( sw/sh > (double)rect->width/rect->height ) {
		/* Slide is too wide.  Letterboxing top/bottom */
		ss->slide_width = rect->width;
		ss->slide_height = rect->width * sh/sw;
	} else {
		/* Letterboxing at sides */
		ss->slide_width = rect->height * sw/sh;
		ss->slide_height = rect->height;
	}

	ss->xoff = (rect->width - ss->slide_width)/2.0;
	ss->yoff = (rect->height - ss->slide_height)/2.0;

	printf("screen %i %i\n", rect->width, rect->height);
	printf("slide %f %f\n", sw, sh);
	printf("rendering slide at %i %i\n", ss->slide_width, ss->slide_height);
	printf("offset %i %i\n", ss->xoff, ss->yoff);

	slideshow_rerender(ss);
}


void sc_slideshow_set_slide(SCSlideshow *ss, SCBlock *ns)
{
	ss->cur_slide = ns;
	slideshow_rerender(ss);
}


SCSlideshow *sc_slideshow_new(struct presentation *p, GtkApplication *app)
{
	GdkDisplay *display;
	int n_monitors;
	SCSlideshow *ss;

	ss = g_object_new(SC_TYPE_SLIDESHOW, NULL);
	if ( ss == NULL ) return NULL;

	ss->blank = 0;
	ss->p = p;
	ss->cur_slide = NULL;
	ss->blank_cursor = NULL;
	ss->surface = NULL;
	ss->app = app;

	ss->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(ss), ss->drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(ss->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(ss->drawingarea),
	                      GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(ss), "destroy",
	                 G_CALLBACK(ssh_destroy_sig), ss);
	g_signal_connect(G_OBJECT(ss), "realize",
	                 G_CALLBACK(ss_realize_sig), ss);
	g_signal_connect(G_OBJECT(ss), "size-allocate",
	                 G_CALLBACK(ss_size_sig), ss);

	g_signal_connect(G_OBJECT(ss->drawingarea), "draw",
			 G_CALLBACK(ss_draw_sig), ss);

	gtk_widget_grab_focus(GTK_WIDGET(ss->drawingarea));

	display = gdk_display_get_default();
	n_monitors = gdk_display_get_n_monitors(display);

	GdkMonitor *mon_ss;
	if ( n_monitors == 1 ) {
		mon_ss = gdk_display_get_primary_monitor(display);
		printf("Single monitor mode\n");
		ss->single_monitor = 1;
	} else {
		mon_ss = gdk_display_get_monitor(display, 1);
		printf("Dual monitor mode\n");
		ss->single_monitor = 0;
	}

	/* Workaround because gtk_window_fullscreen_on_monitor doesn't work */
	GdkRectangle rect;
	gdk_monitor_get_geometry(mon_ss, &rect);
	gtk_window_move(GTK_WINDOW(ss), rect.x, rect.y);
	gtk_window_fullscreen(GTK_WINDOW(ss));

	ss->linked = 1;

	if ( app != NULL ) {
		ss->inhibit_cookie = gtk_application_inhibit(app, GTK_WINDOW(ss),
		                                             GTK_APPLICATION_INHIBIT_IDLE,
		                                             "Presentation slide show is running");
	}

	return ss;
}

