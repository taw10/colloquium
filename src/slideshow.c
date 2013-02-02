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
#include "mainwindow.h"
#include "render.h"


/* Force a redraw of the slideshow */
void redraw_slideshow(struct presentation *p)
{
	gint w, h;

	w = gtk_widget_get_allocated_width(GTK_WIDGET(p->ss_drawingarea));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(p->ss_drawingarea));

	gtk_widget_queue_draw_area(p->ss_drawingarea, 0, 0, w, h);
}


static gint ss_destroy_sig(GtkWidget *widget, struct presentation *p)
{
	p->slideshow = NULL;
	g_object_unref(p->blank_cursor);
	return FALSE;
}


static gboolean ss_draw_sig(GtkWidget *da, cairo_t *cr, struct presentation *p)
{
	GtkAllocation allocation;
	double xoff, yoff;
	double width, height;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_fill(cr);

	if ( !p->ss_blank ) {

		int h;

		h = p->proj_slide_width * p->slide_height / p->slide_width;

		/* Get the overall size */
		gtk_widget_get_allocation(da, &allocation);
		xoff = (allocation.width - p->proj_slide_width)/2.0;
		yoff = (allocation.height - h)/2.0;

		/* Draw the slide from the cache */
		cairo_rectangle(cr, 0.0, 0.0, width, height);
		cairo_set_source_surface(cr, p->cur_proj_slide->rendered_proj,
		                         xoff, yoff);
		cairo_fill(cr);

	}

	return FALSE;
}


void change_proj_slide(struct presentation *p, struct slide *np)
{
	/* If this slide is not being shown on the editor, we can free the
	 * buffers */
	if ( p->cur_proj_slide != p->cur_edit_slide ) {
		free_render_buffers_except_thumb(p->cur_proj_slide);
	}

	p->cur_proj_slide = np;

	/* The slide is already rendered, because the editor always gets there
	 * first, so we only need to do this: */
	redraw_slideshow(p);
}


static gint prev_slide_sig(GtkWidget *widget, struct presentation *p)
{
	int cur_slide_number;
	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == 0 ) return FALSE;
	change_edit_slide(p, p->slides[cur_slide_number-1]);
	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, struct presentation *p)
{
	int cur_slide_number;
	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == p->num_slides-1 ) return FALSE;
	change_edit_slide(p, p->slides[cur_slide_number+1]);
	return FALSE;
}


void end_slideshow(struct presentation *p)
{
	gtk_widget_destroy(p->ss_drawingarea);
	gtk_widget_destroy(p->slideshow);
	p->slideshow = NULL;

	if ( (p->cur_proj_slide != NULL)
	  && (p->cur_proj_slide->rendered_edit != NULL) )
	{
		cairo_surface_destroy(p->cur_proj_slide->rendered_proj);
		p->cur_proj_slide->rendered_proj = NULL;
	}

	p->cur_proj_slide = NULL;
	redraw_editor(p);
}


void toggle_slideshow_link(struct presentation *p)
{
	p->slideshow_linked = 1 - p->slideshow_linked;
	if ( p->slideshow_linked ) {
		change_proj_slide(p, p->cur_edit_slide);
	}
	redraw_editor(p);
}


void check_toggle_blank(struct presentation *p)
{
	if ( p->slideshow != NULL ) {
		//if ( p->prefs->b_splits ) {
			toggle_slideshow_link(p);
		//} else {
		//	p->ss_blank = 1-p->ss_blank;
		//	gdk_window_invalidate_rect(p->ss_drawingarea->window,
		//			           NULL, FALSE);
		//}  FIXME!
	}
}


static gboolean ss_key_press_sig(GtkWidget *da, GdkEventKey *event,
                              struct presentation *p)
{
	switch ( event->keyval ) {

	case GDK_KEY_B :
	case GDK_KEY_b :
		check_toggle_blank(p);
		break;

	case GDK_KEY_Page_Up :
	case GDK_KEY_Up :
		prev_slide_sig(NULL, p);
		break;

	case GDK_KEY_Page_Down :
	case GDK_KEY_Down :
		next_slide_sig(NULL, p);
		break;

	case GDK_KEY_Escape :
		end_slideshow(p);
		break;

	}

	return FALSE;
}


static gboolean ss_realize_sig(GtkWidget *w, struct presentation *p)
{
	GdkWindow *win;

	win = gtk_widget_get_window(w);

	p->blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	gdk_window_set_cursor(GDK_WINDOW(win), p->blank_cursor);

	gtk_window_parse_geometry(GTK_WINDOW(w), p->ss_geom);

	return FALSE;
}


void try_start_slideshow(struct presentation *p)
{
	GtkWidget *n;
	GdkScreen *screen;
	int n_monitors;
	int i;

	/* Presentation already running? */
	if ( p->slideshow != NULL ) return;

	p->ss_blank = 0;

	n = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	p->ss_drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(n), p->ss_drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(p->ss_drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(p->ss_drawingarea),
	                      GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(p->ss_drawingarea), "key-press-event",
			 G_CALLBACK(ss_key_press_sig), p);
	g_signal_connect(G_OBJECT(n), "destroy", G_CALLBACK(ss_destroy_sig), p);
	g_signal_connect(G_OBJECT(n), "realize", G_CALLBACK(ss_realize_sig), p);

	g_signal_connect(G_OBJECT(p->ss_drawingarea), "draw",
			 G_CALLBACK(ss_draw_sig), p);

	gtk_widget_grab_focus(GTK_WIDGET(p->ss_drawingarea));

	screen = gdk_screen_get_default();
	n_monitors = gdk_screen_get_n_monitors(screen);
	for ( i=0; i<n_monitors; i++ ) {

		GdkRectangle rect;
		int w;

		gdk_screen_get_monitor_geometry(screen, i, &rect);
		snprintf(p->ss_geom, 255, "%ix%i+%i+%i",
		         rect.width, rect.height, rect.x, rect.y);

		w = rect.height * p->slide_width/p->slide_height;
		if ( w > rect.width ) w = rect.width;
		p->proj_slide_width = w;

	} /* FIXME: Sensible (configurable) choice of monitor */

	p->slideshow = n;
	p->slideshow_linked = 1;
	gtk_window_fullscreen(GTK_WINDOW(n));
	gtk_widget_show_all(GTK_WIDGET(n));

	//if ( p->prefs->open_notes ) open_notes(p);  FIXME!

	p->cur_proj_slide = p->cur_edit_slide;
}
