/*
 * timer_bar.c
 *
 * Copyright © 2025 Thomas White <taw@bitwiz.me.uk>
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

#include <libintl.h>
#define _(x) gettext(x)

#include "timer.h"
#include "timer_bar.h"

G_DEFINE_FINAL_TYPE(TimerBar, colloquium_timer_bar, GTK_TYPE_WIDGET)

static void timer_bar_snapshot(GtkWidget *da, GtkSnapshot *snapshot);


static void colloquium_timer_bar_class_init(TimerBarClass *klass)
{
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->snapshot = timer_bar_snapshot;
}


static void colloquium_timer_bar_init(TimerBar *e)
{
}


static void timer_bar_snapshot(GtkWidget *da, GtkSnapshot *snapshot)
{
    TimerBar *b = COLLOQUIUM_TIMER_BAR(da);
    int w, h;
    cairo_t *cr;
    double total_time;
    double progress_time;
    double elapsed_time;

    w = gtk_widget_get_width(da);
    h = gtk_widget_get_height(da);

    /* Overall background */
    GdkRGBA col = {1.0, 1.0, 1.0, 1.0};
    gtk_snapshot_append_color(snapshot, &col, &GRAPHENE_RECT_INIT(0,0,w,h));

    cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0,0,w,h));

    total_time = colloquium_timer_get_main_time(b->timer);
    progress_time = total_time * colloquium_timer_get_max_progress_fraction(b->timer);
    elapsed_time = colloquium_timer_get_elapsed_main_time(b->timer);

    /* x-coordinates are seconds, y-coordinates are fraction of height */
    cairo_scale(cr, w/total_time, h);

    cairo_rectangle(cr, 0.0, 0.0, elapsed_time, 1.0);
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
    cairo_fill(cr);

    if ( progress_time > elapsed_time ) {
        /* Ahead of time */
        cairo_rectangle(cr, elapsed_time, 0.0, progress_time - elapsed_time, 1.0);
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
        cairo_fill(cr);
    } else {
        /* Behind time */
        cairo_rectangle(cr, progress_time, 0.0, elapsed_time - progress_time, 1.0);
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_fill(cr);
    }

    cairo_destroy(cr);

    cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0,0,w,h));

    /* Current position marker (even if behind high water mark) */
    cairo_set_source_rgb(cr, 0.6, 0.0, 0.8);
    cairo_move_to(cr, w*colloquium_timer_get_progress_fraction(b->timer), 0.0);
    cairo_line_to(cr, w*colloquium_timer_get_progress_fraction(b->timer), h);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    if ( !colloquium_timer_get_running(b->timer) ) {
        cairo_move_to(cr, 0.0, 0.8*h);
        cairo_set_font_size(cr, 0.8*h);
        cairo_select_font_face(cr, "sans-serif",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_show_text(cr, _("Timer is NOT running!"));
    }

    cairo_destroy(cr);
}


static gboolean timeout_event(gpointer data)
{
    TimerBar *n = data;
    gtk_widget_queue_draw(GTK_WIDGET(n));
    return TRUE;
}


static void clock_event(Timer *t, TimerBar *b)
{
    gtk_widget_queue_draw(GTK_WIDGET(b));
}


static void close_clock_sig(GtkWidget *w, TimerBar *n)
{
    g_source_remove(n->timer_id);
    g_signal_handlers_disconnect_by_data(G_OBJECT(n->timer), n);
}


GtkWidget *colloquium_timer_bar_new(Timer *t)
{
    TimerBar *b;

    b = COLLOQUIUM_TIMER_BAR(g_object_new(COLLOQUIUM_TYPE_TIMER_BAR, NULL));
    gtk_widget_set_size_request(GTK_WIDGET(b), 0, 32);

    b->timer = t;
    b->timer_id = g_timeout_add_seconds(1, timeout_event, b);

    g_signal_connect(G_OBJECT(t), "clock-event", G_CALLBACK(clock_event), b);
    g_signal_connect(G_OBJECT(b), "destroy", G_CALLBACK(close_clock_sig), b);

    return GTK_WIDGET(b);
}
