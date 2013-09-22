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

	int width;   /* Number of slides across */
	int height;  /* Number of slides vertically */
	struct presentation *p;

	int tw;
	int th;
	int bw;

	int selection;
	int dragging;
	int drag_preview_pending;
	int have_drag_data;
	int drag_highlight;
};


static gint close_slidesorter_sig(GtkWidget *w, struct presentation *p)
{
	p->slide_sorter = NULL;
	return FALSE;
}


static void redraw_slidesorter(struct slide_sorter *n)
{
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(n->da));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(n->da));
	gtk_widget_queue_draw_area(n->da, 0, 0, w, h);
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, struct slide_sorter *n)
{
	int width, height;
	int i;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_translate(cr, n->bw, n->bw);  /* Border */

	for ( i=0; i<n->p->num_slides; i++ ) {

		int x = i % n->width;
		int y = i / n->width;
		struct slide *s = n->p->slides[i];

		cairo_save(cr);

		cairo_translate(cr, x*(n->tw+2*n->bw), y*(n->th+2*n->bw));

		if ( i == n->selection ) {

			cairo_save(cr);
			cairo_rectangle(cr, 0.0,  0.0, n->tw+2*n->bw,
			                               n->th+2*n->bw);
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.4);
			cairo_fill(cr);
			cairo_restore(cr);

		}

		cairo_translate(cr, n->bw, n->bw);  /* Border */
		cairo_rectangle(cr, 0.0, 0.0, n->tw, n->th);
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
		cairo_fill_preserve(cr);
		if ( s->rendered_thumb != NULL ) {
			cairo_set_source_surface(cr, s->rendered_thumb,
			                         0.0, 0.0);
		}
		cairo_fill(cr);

		cairo_rectangle(cr, 0.5, 0.5, n->tw, n->th);
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);

		cairo_restore(cr);

	}

	return FALSE;
}


static void size_sig(GtkWidget *widget, GdkRectangle *size,
                     struct slide_sorter *n)
{
	int w, h;

	w = gtk_widget_get_allocated_width(n->da);
	n->width = (w-2*n->bw) / (n->tw+2*n->bw);
	if ( n->width < 1 ) n->width = 1;

	n->height = n->p->num_slides / n->width;

	if ( n->p->num_slides % n->width != 0 ) n->height++;

	h = n->height * (n->th+2*n->bw) + 2*n->bw;

	gtk_widget_set_size_request(n->da, n->tw+2*n->bw, h);
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 struct slide_sorter *n)
{
	int x, y;

	x = (event->x - n->bw) / (n->tw + 2*n->bw);
	y = (event->y - n->bw) / (n->th + 2*n->bw);

	if ( x >= n->width ) x = n->width-1;
	if ( y >= n->height ) y = n->height-1;

	n->selection = y*n->width + x;
	if ( n->selection >= n->p->num_slides ) {
		n->selection = n->p->num_slides - 1;
	}

	redraw_slidesorter(n);

	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                           struct slide_sorter *n)
{
	if ( !n->dragging ) {

		GtkTargetList *list;
		GtkTargetEntry targets[1];

		targets[0].target = "application/vnd.colloquium-slide";
		targets[0].flags = 0;
		targets[0].info = 1;

		list = gtk_target_list_new(targets, 1);
		gtk_drag_begin(da, list, GDK_ACTION_COPY | GDK_ACTION_MOVE,
		               1, (GdkEvent *)event);
		gtk_target_list_unref(list);

		n->dragging = 1;

	}

	gdk_event_request_motions(event);

	return FALSE;
}


static gboolean button_release_sig(GtkWidget *da, GdkEventButton *event,
                                   struct slide_sorter *n)
{
	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, struct slide_sorter *n)
{
	GdkAtom target;

	/* If we haven't already requested the data, do so now */
	if ( !n->drag_preview_pending && !n->have_drag_data ) {

		target = gtk_drag_dest_find_target(widget, drag_context, NULL);

		if ( target != GDK_NONE ) {

			n->drag_preview_pending = 1;
			/* Note: the dnd_get and dnd_receive signals occur
			 * before this call returns */
			gtk_drag_get_data(widget, drag_context, target, time);

		} else {

			n->drag_preview_pending = 0;
			gdk_drag_status(drag_context, 0, time);

		}

	}

	if ( n->have_drag_data ) {

		gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
		/* Draw drag box */
		n->have_drag_data = 1;

	}

	return TRUE;
}



static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                     gint x, gint y, guint time, struct slide_sorter *n)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target(widget, drag_context, NULL);

	if ( target == GDK_NONE ) {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
	} else {
		gtk_drag_get_data(widget, drag_context, target, time);
	}

	return TRUE;
}


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, struct slide_sorter *n)
{
	if ( n->drag_preview_pending ) {

		/* In theory: do something with the data to generate a
		 * proper preview. */

		n->have_drag_data = 1;
		n->drag_preview_pending = 0;
		gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);

	} else {

		const guchar *sc;

		sc = gtk_selection_data_get_data(seldata);

		n->dragging = 0;
		gtk_drag_finish(drag_context, TRUE, TRUE, time);

		printf("Got SC: '%s'\n", sc);

	}

}


static void dnd_get(GtkWidget *widget, GdkDragContext *drag_context,
                    GtkSelectionData *seldata, guint info, guint time,
                    struct slide_sorter *n)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target(widget, drag_context, NULL);

	if ( target != GDK_NONE ) {

		char *sc;
		sc = packed_sc(n->p->slides[n->selection]->top, n->p->ss);
		printf("Sending SC: '%s'\n", sc);
		gtk_selection_data_set(seldata, target, 8, (guchar *)sc,
		                       strlen(sc));

	}
}


static void dnd_leave(GtkWidget *widget, GdkDragContext *drag_context,
                      guint time, struct slide_sorter *n)
{
	n->have_drag_data = 0;
}


static void dnd_end(GtkWidget *widget, GdkDragContext *drag_context,
                    struct slide_sorter *n)
{
	n->dragging = 0;
	n->have_drag_data = 0;
	n->drag_preview_pending = 0;
}


void open_slidesorter(struct presentation *p)
{
	struct slide_sorter *n;
	GtkWidget *sw;
	GtkTargetEntry targets[1];

	if ( p->slide_sorter != NULL ) return;  /* Already open */

	n = malloc(sizeof(struct slide_sorter));
	if ( n == NULL ) return;
	p->slide_sorter = n;

	n->p = p;
	n->width = 6;
	n->bw = 5;
	n->selection = 0;
	n->tw = p->thumb_slide_width;
	n->th = (p->slide_height/p->slide_width) * p->thumb_slide_width;
	n->drag_preview_pending = 0;
	n->have_drag_data = 0;
	n->dragging = 0;

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
	g_signal_connect(G_OBJECT(n->window),
	                 "size-allocate", G_CALLBACK(size_sig), n);
	g_signal_connect(G_OBJECT(n->window), "destroy",
	                 G_CALLBACK(close_slidesorter_sig), p);

	g_signal_connect(G_OBJECT(n->da), "button-press-event",
	                 G_CALLBACK(button_press_sig), n);
	g_signal_connect(G_OBJECT(n->da), "motion-notify-event",
	                 G_CALLBACK(motion_sig), n);
	g_signal_connect(G_OBJECT(n->da), "button-release-event",
	                 G_CALLBACK(button_release_sig), n);

	/* Drag and drop */
	targets[0].target = "application/vnd.colloquium-slide";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(n->da, 0, targets, 1,
	                  GDK_ACTION_MOVE);
	g_signal_connect(n->da, "drag-data-received",
	                 G_CALLBACK(dnd_receive), n);
	g_signal_connect(n->da, "drag-data-get", G_CALLBACK(dnd_get), n);
	g_signal_connect(n->da, "drag-motion", G_CALLBACK(dnd_motion), n);
	g_signal_connect(n->da, "drag-drop", G_CALLBACK(dnd_drop), n);
	g_signal_connect(n->da, "drag-leave", G_CALLBACK(dnd_leave), n);
	g_signal_connect(n->da, "drag-end", G_CALLBACK(dnd_end), n);

	gtk_widget_set_can_focus(GTK_WIDGET(n->da), TRUE);
	gtk_widget_add_events(GTK_WIDGET(n->da),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	gtk_widget_show_all(n->window);
}
