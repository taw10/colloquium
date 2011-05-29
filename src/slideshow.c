/*
 * slideshow.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2011 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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
#include "slide_render.h"
#include "mainwindow.h"


static gint ss_destroy_sig(GtkWidget *widget, struct presentation *p)
{
	p->slideshow = NULL;
	return FALSE;
}


static gboolean ss_expose_sig(GtkWidget *da, GdkEventExpose *event,
                           struct presentation *p)
{
	cairo_t *cr;
	GtkAllocation allocation;
	double xoff, yoff;

	check_redraw_slide(p->view_slide);

	cr = gdk_cairo_create(da->window);

	/* Overall background */
	cairo_rectangle(cr, event->area.x, event->area.y,
	                event->area.width, event->area.height);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_fill(cr);

	/* Get the overall size */
	gtk_widget_get_allocation(da, &allocation);
	xoff = (allocation.width - p->slide_width)/2.0;
	yoff = (allocation.height - p->slide_height)/2.0;
	p->border_offs_x = xoff;  p->border_offs_y = yoff;

	/* Draw the slide from the cache */
	cairo_rectangle(cr, event->area.x, event->area.y,
	                event->area.width, event->area.height);
	cairo_set_source_surface(cr, p->view_slide->render_cache, xoff, yoff);
	cairo_fill(cr);

	cairo_destroy(cr);

	return FALSE;
}


static gint prev_slide_sig(GtkWidget *widget, struct presentation *p)
{
	if ( p->view_slide_number == 0 ) return FALSE;

	p->view_slide_number--;
	p->view_slide = p->slides[p->view_slide_number];

	gdk_window_invalidate_rect(p->ss_drawingarea->window, NULL, FALSE);

	notify_slide_changed(p);

	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, struct presentation *p)
{
	if ( p->view_slide_number == p->num_slides-1 ) return FALSE;

	p->view_slide_number++;
	p->view_slide = p->slides[p->view_slide_number];

	gdk_window_invalidate_rect(p->ss_drawingarea->window, NULL, FALSE);

	notify_slide_changed(p);

	return FALSE;
}


static gboolean ss_key_press_sig(GtkWidget *da, GdkEventKey *event,
                              struct presentation *p)
{
	switch ( event->keyval ) {

	case GDK_KEY_Page_Up :
		prev_slide_sig(NULL, p);
		break;

	case GDK_KEY_Page_Down :
		next_slide_sig(NULL, p);
		break;

	case GDK_KEY_Escape :
		gtk_widget_destroy(p->ss_drawingarea);
		gtk_widget_destroy(p->slideshow);
		break;

	}

	return FALSE;
}


void try_start_slideshow(struct presentation *p)
{
	GtkWidget *n;

	/* Presentation already running? */
	if ( p->slideshow != NULL ) return;

	n = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	p->ss_drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(n), p->ss_drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(p->ss_drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(p->ss_drawingarea),
	                      GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(p->ss_drawingarea), "key-press-event",
			 G_CALLBACK(ss_key_press_sig), p);
	g_signal_connect(G_OBJECT(p->ss_drawingarea), "expose-event",
			 G_CALLBACK(ss_expose_sig), p);
	g_signal_connect(G_OBJECT(n), "destroy", G_CALLBACK(ss_destroy_sig), p);

	gtk_widget_grab_focus(GTK_WIDGET(p->ss_drawingarea));

	p->slideshow = n;
	gtk_window_fullscreen(GTK_WINDOW(n));
	gtk_widget_show_all(GTK_WIDGET(n));
}
