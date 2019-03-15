/*
 * gtkslideview.c
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
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <libintl.h>
#define _(x) gettext(x)

#include <presentation.h>
#include <slide_render_cairo.h>

//#include "slide_window.h"
#include "gtkslideview.h"
#include "slide_priv.h"
//#include "slideshow.h"


G_DEFINE_TYPE_WITH_CODE(GtkSlideView, gtk_slide_view, GTK_TYPE_DRAWING_AREA,
                        NULL)


static gboolean resize_sig(GtkWidget *widget, GdkEventConfigure *event,
                           GtkSlideView *e)
{
	double sx, sy;
	double aw, ah;
	double log_w, log_h;
	Stylesheet *ss;

	ss = presentation_get_stylesheet(e->p);
	if ( slide_get_logical_size(e->slide, ss, &log_w, &log_h) ) {
		fprintf(stderr, "Failed to get logical size\n");
		return FALSE;
	}

	e->w = event->width;
	e->h = event->height;
	sx = (double)e->w / log_w;
	sy = (double)e->h / log_h;
	e->view_scale = (sx < sy) ? sx : sy;

	/* Actual size (in device units) */
	aw = e->view_scale * log_w;
	ah = e->view_scale * log_h;

	e->border_offs_x = (event->width - aw)/2.0;
	e->border_offs_y = (event->height - ah)/2.0;

	e->visible_height = event->height;
	e->visible_width = event->width;

	//update_size(e);

	return FALSE;
}


static void emit_change_sig(GtkSlideView *e)
{
	g_signal_emit_by_name(e, "changed");
}


static void gtk_slide_view_class_init(GtkSlideViewClass *klass)
{
	g_signal_new("changed", GTK_TYPE_SLIDE_VIEW, G_SIGNAL_RUN_LAST, 0,
	             NULL, NULL, NULL, G_TYPE_NONE, 0);
}


static void gtk_slide_view_init(GtkSlideView *e)
{
}


static void redraw(GtkSlideView *e)
{
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));
	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


static gint destroy_sig(GtkWidget *window, GtkSlideView *e)
{
	return 0;
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, GtkSlideView *e)
{
	PangoContext *pc;

	/* Ultimate background */
	if ( e->bg_pixbuf != NULL ) {
		gdk_cairo_set_source_pixbuf(cr, e->bg_pixbuf, 0.0, 0.0);
		cairo_pattern_t *patt = cairo_get_source(cr);
		cairo_pattern_set_extend(patt, CAIRO_EXTEND_REPEAT);
		cairo_paint(cr);
	} else {
		cairo_set_source_rgba(cr, 0.8, 0.8, 1.0, 1.0);
		cairo_paint(cr);
	}

	cairo_translate(cr, e->border_offs_x, e->border_offs_y);
	cairo_translate(cr, -e->h_scroll_pos, -e->v_scroll_pos);
	cairo_scale(cr, e->view_scale, e->view_scale);

	/* Contents */
	pc = pango_cairo_create_context(cr);
	slide_render_cairo(e->slide, cr, presentation_get_imagestore(e->p),
	                   presentation_get_stylesheet(e->p),
	                   presentation_get_slide_number(e->p, e->slide),
	                   pango_language_get_default(), pc);
	g_object_unref(pc);

	/* Editing overlay */
	//draw_overlay(cr, e);

	return FALSE;
}


static gint realise_sig(GtkWidget *da, GtkSlideView *e)
{
	GdkWindow *win;

	/* Keyboard and input method stuff */
	e->im_context = gtk_im_multicontext_new();
	win = gtk_widget_get_window(GTK_WIDGET(e));
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(e->im_context), win);
	gdk_window_set_accept_focus(win, TRUE);
	//g_signal_connect(G_OBJECT(e->im_context), "commit", G_CALLBACK(im_commit_sig), e);
	//g_signal_connect(G_OBJECT(e), "key-press-event", G_CALLBACK(key_press_sig), e);

	return FALSE;
}


static void update_size_request(GtkSlideView *e)
{
	gtk_widget_set_size_request(GTK_WIDGET(e), 0, e->h + 2.0*e->min_border);
}


void gtk_slide_view_set_scale(GtkSlideView *e, double scale)
{
	e->view_scale = 1.0;
}


void gtk_slide_view_set_slide(GtkWidget *e, Slide *slide)
{
	GTK_SLIDE_VIEW(e)->slide = slide;
	redraw(GTK_SLIDE_VIEW(e));
}


GtkWidget *gtk_slide_view_new(Presentation *p, Slide *slide)
{
	GtkSlideView *sv;
	GtkTargetEntry targets[1];
	GError *err;

	sv = g_object_new(GTK_TYPE_SLIDE_VIEW, NULL);

	sv->p = p;
	sv->slide = slide;
	sv->w = 100;
	sv->h = 100;
	sv->border_offs_x = 0;
	sv->border_offs_y = 0;
	sv->min_border = 0.0;
	sv->h_scroll_pos = 0;
	sv->v_scroll_pos = 0;
	sv->view_scale = 1.0;

	err = NULL;
	sv->bg_pixbuf = gdk_pixbuf_new_from_resource("/uk/me/bitwiz/Colloquium/sky.png",
	                                                   &err);
	if ( sv->bg_pixbuf == NULL ) {
		fprintf(stderr, _("Failed to load background: %s\n"),
		        err->message);
	}

	gtk_widget_set_size_request(GTK_WIDGET(sv),
	                            sv->w, sv->h);

	g_signal_connect(G_OBJECT(sv), "destroy",
	                 G_CALLBACK(destroy_sig), sv);
	g_signal_connect(G_OBJECT(sv), "realize",
	                 G_CALLBACK(realise_sig), sv);
	//g_signal_connect(G_OBJECT(sv), "button-press-event",
	//                 G_CALLBACK(button_press_sig), sv);
	//g_signal_connect(G_OBJECT(sv), "button-release-event",
	//                 G_CALLBACK(button_release_sig), sv);
	//g_signal_connect(G_OBJECT(sv), "motion-notify-event",
	//                 G_CALLBACK(motion_sig), sv);
	g_signal_connect(G_OBJECT(sv), "configure-event",
	                 G_CALLBACK(resize_sig), sv);

	/* Drag and drop */
	//targets[0].target = "text/uri-list";
	//targets[0].flags = 0;
	//targets[0].info = 1;
	//gtk_drag_dest_set(GTK_WIDGET(sv), 0, targets, 1,
	//                  GDK_ACTION_PRIVATE);
	//g_signal_connect(sv, "drag-data-received",
	//                 G_CALLBACK(dnd_receive), sv);
	//g_signal_connect(sv, "drag-motion",
	//                 G_CALLBACK(dnd_motion), sv);
	//g_signal_connect(sv, "drag-drop",
	//                 G_CALLBACK(dnd_drop), sv);
	//g_signal_connect(sv, "drag-leave",
	//                 G_CALLBACK(dnd_leave), sv);

	gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
	gtk_widget_add_events(GTK_WIDGET(sv),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
	                       | GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(sv), "draw",
			 G_CALLBACK(draw_sig), sv);

	gtk_widget_grab_focus(GTK_WIDGET(sv));

	gtk_widget_show(GTK_WIDGET(sv));

	return GTK_WIDGET(sv);
}
