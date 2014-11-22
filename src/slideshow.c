/*
 * slideshow.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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
#include "slide_window.h"
#include "render.h"
#include "pr_clock.h"
#include "inhibit_screensaver.h"
#include "frame.h"

struct _slideshow
{
	struct presentation *p;
	SlideWindow         *slide_window;  /* Slide window controlling us */
	struct slide        *cur_slide;
	GtkWidget           *window;
	GtkWidget           *drawingarea;
	GdkCursor           *blank_cursor;
	int                  blank;
	char                 geom[256];
	int                  slide_width;
	int                  slide_height;
	struct inhibit_sys  *inhibit;
	int                  linked;
	cairo_surface_t     *surface;
	struct frame         top;
};


/* Force a redraw of the slideshow */
void redraw_slideshow(SlideShow *ss)
{
	gint w, h;

	w = gtk_widget_get_allocated_width(GTK_WIDGET(ss->drawingarea));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(ss->drawingarea));

	gtk_widget_queue_draw_area(ss->drawingarea, 0, 0, w, h);
}


void slideshow_rerender(SlideShow *ss)
{
	int n;

	n = slide_number(ss->p, ss->cur_slide);
	ss->surface = render_sc(ss->cur_slide->scblocks,
	                        ss->slide_width, ss->slide_height,
	                        ss->p->slide_width, ss->p->slide_height,
	                        ss->p->stylesheet,
	                        ss->p->is, ISZ_SLIDESHOW, n);
}


static gint ss_destroy_sig(GtkWidget *widget, SlideShow *ss)
{
	g_object_unref(ss->p->slideshow->blank_cursor);
	ss->p->slideshow = NULL;
	return FALSE;
}


static gboolean ss_draw_sig(GtkWidget *da, cairo_t *cr, SlideShow *ss)
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


void change_proj_slide(SlideShow *ss, struct slide *np)
{
	ss->cur_slide = np;

	notify_clock_slide_changed(ss->p, np);

	slideshow_rerender(ss);
	redraw_slideshow(ss);
}


static gint prev_slide_sig(GtkWidget *widget, SlideShow *ss)
{
	change_slide_backwards(ss->slide_window);
	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, SlideShow *ss)
{
	change_slide_forwards(ss->slide_window);
	return FALSE;
}


void end_slideshow(SlideShow *ss)
{
	if ( ss->inhibit != NULL ) do_inhibit(ss->inhibit, 0);
	gtk_widget_destroy(ss->drawingarea);
	gtk_widget_destroy(ss->window);
	ss->p->slideshow = NULL;
	slidewindow_redraw(ss->slide_window);
	free(ss);
}


void toggle_slideshow_link(SlideShow *ss)
{
	ss->linked = 1 - ss->linked;
	if ( ss->linked ) {
		change_proj_slide(ss, slidewindow_get_slide(ss->slide_window));
	}
	slidewindow_redraw(ss->slide_window);
}


int slideshow_linked(SlideShow *ss)
{
	if ( ss == NULL ) return 0;
	return ss->linked;
}


void check_toggle_blank(SlideShow *ss)
{
	if ( ss != NULL ) {
		//if ( p->prefs->b_splits ) {
			toggle_slideshow_link(ss);
		//} else {
		//	p->ss_blank = 1-p->ss_blank;
		//	gdk_window_invalidate_rect(p->ss_drawingarea->window,
		//			           NULL, FALSE);
		//}  FIXME!
	}
}


static gboolean ss_key_press_sig(GtkWidget *da, GdkEventKey *event,
                                 SlideShow *ss)
{
	switch ( event->keyval )
	{

		case GDK_KEY_B :
		case GDK_KEY_b :
		check_toggle_blank(ss);
		break;

		case GDK_KEY_Page_Up :
		case GDK_KEY_Up :
		prev_slide_sig(NULL, ss);
		break;

		case GDK_KEY_Page_Down :
		case GDK_KEY_Down :
		next_slide_sig(NULL, ss);
		break;

		case GDK_KEY_Escape :
		end_slideshow(ss);
		break;

	}

	return FALSE;
}


static gboolean ss_realize_sig(GtkWidget *w, SlideShow *ss)
{
	GdkWindow *win;

	win = gtk_widget_get_window(w);

	ss->blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
	gdk_window_set_cursor(GDK_WINDOW(win), ss->blank_cursor);

	gtk_window_parse_geometry(GTK_WINDOW(w), ss->geom);
	slideshow_rerender(ss);

	return FALSE;
}


struct slide *slideshow_slide(SlideShow *ss)
{
	if ( ss == NULL ) return NULL;
	return ss->cur_slide;
}


SlideShow *try_start_slideshow(SlideWindow *sw, struct presentation *p)
{
	GdkScreen *screen;
	int n_monitors;
	int i;
	SlideShow *ss;
	double slide_width = 1024.0; /* Logical slide size */
	double slide_height = 768.0; /* FIXME: Should come from slide */
	ss = calloc(1, sizeof(SlideShow));
	if ( ss == NULL ) return NULL;

	ss->slide_window = sw;
	ss->blank = 0;
	ss->p = p;
	ss->cur_slide = slidewindow_get_slide(sw);

	if ( ss->inhibit == NULL ) {
		ss->inhibit = inhibit_prepare();
	}

	ss->top.children = NULL;
	ss->top.num_children = 0;
	ss->top.max_children = 0;
	ss->top.lines = NULL;
	ss->top.n_lines = 0;
	ss->top.max_lines = 0;
	ss->top.pad_l = 0;
	ss->top.pad_r = 0;
	ss->top.pad_t = 0;
	ss->top.pad_b = 0;
	ss->top.x = 0.0;
	ss->top.y = 0.0;
	ss->top.w = slide_width;
	ss->top.h = slide_height;
	ss->top.grad = GRAD_NONE;
	ss->top.bgcol[0] = 1.0;
	ss->top.bgcol[1] = 1.0;
	ss->top.bgcol[2] = 1.0;
	ss->top.bgcol[3] = 1.0;

	ss->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	ss->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(ss->window), ss->drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(ss->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(ss->drawingarea),
	                      GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(ss->drawingarea), "key-press-event",
			 G_CALLBACK(ss_key_press_sig), ss);
	g_signal_connect(G_OBJECT(ss->window), "destroy",
	                 G_CALLBACK(ss_destroy_sig), ss);
	g_signal_connect(G_OBJECT(ss->window), "realize",
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
	gtk_window_fullscreen(GTK_WINDOW(ss->window));
	gtk_widget_show_all(GTK_WIDGET(ss->window));

	if ( ss->inhibit != NULL ) do_inhibit(ss->inhibit, 1);

	//notify_clock_slide_changed(p, ss->cur_slide); FIXME

	return ss;
}

