/*
 * thumbnailwidget.c
 *
 * Copyright © 2025 Thomas White <taw@bitwiz.org.uk>
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
#include <libintl.h>
#define _(x) gettext(x)

#include "thumbnailwidget.h"
#include "narrative_window.h"
#include "slide_render_cairo.h"
#include "slide.h"
#include "slide_window.h"


G_DEFINE_FINAL_TYPE(Thumbnail, colloquium_thumbnail, GTK_TYPE_WIDGET)


static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot);

static void colloquium_thumbnail_class_init(ThumbnailClass *klass)
{
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->snapshot = thumbnail_snapshot;
}


static void colloquium_thumbnail_init(Thumbnail *e)
{
}


static void click_sig(GtkGestureClick *self, int n_press, gdouble x, gdouble y, gpointer vp)
{
    Thumbnail *th = vp;

    if ( n_press != 2 ) return;

    if ( th->nw->show == NULL ) {
        if ( th->nw->n_slidewindows < 16 ) {
            SlideWindow *sw = slide_window_new(th->nw->n, th->slide, th->nw, th->nw->app);
            th->nw->slidewindows[th->nw->n_slidewindows++] = sw;
            g_signal_connect(G_OBJECT(sw), "destroy",
                             G_CALLBACK(slide_window_closed_sig), th->nw);
            gtk_window_present(GTK_WINDOW(sw));
        } else {
            fprintf(stderr, _("Too many slide windows\n"));
        }
    } else {
        sc_slideshow_set_slide(th->nw->show, th->slide);
    }
}


static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot)
{
    double logical_w, logical_h;
    Thumbnail *th = COLLOQUIUM_THUMBNAIL(da);
    cairo_t *cr;
    int w, h;

    w = gtk_widget_get_width(da);
    h = gtk_widget_get_height(da);

    cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0,0,w,h));

    slide_get_logical_size(th->slide, &logical_w, &logical_h);
    cairo_scale(cr, (double)w/logical_w, (double)h/logical_h);
    slide_render_cairo(th->slide, cr);

    cairo_rectangle(cr, 0, 0, logical_w, logical_h);
    cairo_set_line_width(cr, 2);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_stroke(cr);
    cairo_destroy(cr);
}


GtkWidget *thumbnail_new(Slide *slide, NarrativeWindow *nw)
{
    Thumbnail *th;
    double w, h;
    GtkGesture *evc;

    th = g_object_new(COLLOQUIUM_TYPE_THUMBNAIL, NULL);
    th->nw = nw;
    th->slide = slide;

    slide_get_logical_size(th->slide, &w, &h);
    gtk_widget_set_size_request(GTK_WIDGET(th), 320*w/h, 320);

    th->cursor = gdk_cursor_new_from_name("pointer", NULL);
    gtk_widget_set_cursor(GTK_WIDGET(th), th->cursor);

    evc = gtk_gesture_click_new();
    gtk_widget_add_controller(GTK_WIDGET(th), GTK_EVENT_CONTROLLER(evc));
    g_signal_connect(G_OBJECT(evc), "pressed", G_CALLBACK(click_sig), th);

    return GTK_WIDGET(th);
}


Slide *thumbnail_get_slide(Thumbnail *th)
{
    return th->slide;
}
