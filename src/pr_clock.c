/*
 * pr_clock.c
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
#include <assert.h>
#include <gtk/gtk.h>
#include <libintl.h>
#define _(x) gettext(x)

#include "pr_clock.h"


struct pr_clock
{
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *startbutton;
    GtkWidget *da;
    GtkWidget *wallclock;
    GtkWidget *elapsed;
    GtkWidget *remaining;
    GtkWidget *status;
    GTimeZone *tz;

    GDateTime *start;
    double time_elapsed_at_start;
    guint timer_id;

    int running;
    double time_allowed;
    double time_elapsed;
    int pos;
    int end;
    int pos_reached;

    double t;
    double tf;

    PRClock **delete_ptr;
};


static char *format_span(int n)
{
    char tmp[32];
    int hours, mins, sec;
    char *s;

    if ( n < 0 ) {
        s = "-";
        n = -n;
    } else {
        s = "";
    }

    sec = n % 60;
    mins = ((n-sec) % (60*60))/60;
    hours = (n-sec-mins) / (60*60);

    snprintf(tmp, 31, "%s%i:%02i:%02i", s, hours, mins, sec);

    return strdup(tmp);
}


static char *format_span_nice(int n)
{
    char tmp[64];
    int hours, mins, sec;
    char *s;

    if ( n < 0 ) {
        s = "behind";
        n = -n;
    } else {
        s = "ahead";
    }

    sec = n % 60;
    mins = ((n-sec) % (60*60))/60;
    hours = (n-sec-mins) / (60*60);

    if ( n <= 60 ) {
        snprintf(tmp, 63, "%i seconds %s", n, s);
        return strdup(tmp);
    }

    if ( n < 60*60 ) {
        snprintf(tmp, 63, "%i min %i seconds %s", mins, sec, s);
        return strdup(tmp);
    }

    snprintf(tmp, 63, "%i hours, %i min, %i seconds %s",
             hours, mins, sec, s);
    return strdup(tmp);
}


static gboolean update_clock(gpointer data)
{
    struct pr_clock *n = data;
    gchar *d;
    GDateTime *dt;
    GTimeSpan sp;
    double time_remaining;
    double delta;
    char *tmp;

    dt = g_date_time_new_now(n->tz);

    if ( n->running ) {

        sp = g_date_time_difference(dt, n->start);
        n->time_elapsed = n->time_elapsed_at_start +
                          sp / G_TIME_SPAN_SECOND;

        time_remaining = n->time_allowed - n->time_elapsed;

        tmp = format_span(n->time_elapsed);
        gtk_label_set_text(GTK_LABEL(n->elapsed), tmp);
        free(tmp);

        tmp = format_span(time_remaining);
        gtk_label_set_text(GTK_LABEL(n->remaining), tmp);
        free(tmp);

    } else {

        n->time_elapsed = n->time_elapsed_at_start;

        time_remaining = n->time_allowed - n->time_elapsed;

        tmp = format_span(n->time_elapsed);
        gtk_label_set_text(GTK_LABEL(n->elapsed), tmp);
        free(tmp);

        tmp = format_span(time_remaining);
        gtk_label_set_text(GTK_LABEL(n->remaining), tmp);
        free(tmp);

    }

    d = g_date_time_format(dt, "%H:%M:%S");
    g_date_time_unref(dt);

    gtk_label_set_text(GTK_LABEL(n->wallclock), d);
    free(d);

    n->t = n->time_elapsed / n->time_allowed;

    if ( n->time_allowed == 0.0 ) n->t = 0.0;
    if ( n->time_elapsed > n->time_allowed ) n->t = 1.0;

    if ( n->end > 0 ) {
        n->tf = (double)n->pos_reached / (n->end-1);
    } else {
        n->tf = 0.0;
    }

    delta = (n->tf - n->t)*n->time_allowed;
    tmp = format_span_nice(delta);
    gtk_label_set_text(GTK_LABEL(n->status), tmp);
    free(tmp);

    gtk_widget_queue_draw(n->da);

    return TRUE;
}


void pr_clock_set_pos(PRClock *n, int pos, int end)
{
    if ( n == NULL ) return;
    n->pos = pos;
    if ( n->pos > n->pos_reached ) {
        n->pos_reached = pos;
    }
    n->end = end;
    update_clock(n);
}


static gint close_clock_sig(GtkWidget *w, PRClock *n)
{
    g_source_remove(n->timer_id);
    *n->delete_ptr = NULL;
    if ( n->start != NULL ) g_date_time_unref(n->start);
    g_time_zone_unref(n->tz);
    free(n);
    return FALSE;
}


static void clock_draw_sig(GtkDrawingArea *da, cairo_t *cr,
        int width, int height, gpointer vp)
{
    double s;
    double ff;
    struct pr_clock *n = vp;

    s = width-20.0;

    /* Overall background */
    cairo_rectangle(cr, 10.0, 0.0, s, height);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_fill(cr);

    cairo_rectangle(cr, 10.0, 0.0, s*n->t, height);
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
    cairo_fill(cr);

    if ( n->tf > n->t ) {
        cairo_rectangle(cr, 10.0+s*n->t, 0.0, (n->tf - n->t)*s, height);
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
        cairo_fill(cr);
    } else {
        cairo_rectangle(cr, 10.0+s*n->t, 0.0, (n->tf - n->t)*s, height);
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_fill(cr);
    }

    ff = (double)n->pos / (n->end-1);
    if ( n->end == 1 ) ff = 0.0;
    cairo_move_to(cr, 10.0+ff*s, 0.0);
    cairo_line_to(cr, 10.0+ff*s, height);
    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
    cairo_stroke(cr);

    if ( !n->running ) {
        cairo_move_to(cr, 10.0, height*0.8);
        cairo_set_font_size(cr, height*0.8);
        cairo_select_font_face(cr, "sans-serif",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_show_text(cr, _("Timer is NOT running!"));
    }
}


static void set_sig(GtkEditable *w, struct pr_clock *n)
{
    gchar *t;
    char *check;

    t = gtk_editable_get_chars(GTK_EDITABLE(n->entry), 0, -1);
    n->time_allowed = 60.0 * strtod(t, &check);
    if ( check == t ) {
        fprintf(stderr, "Invalid time '%s'\n", t);
        n->time_allowed = 0.0;
    }
    g_free(t);

    update_clock(n);
}


static gboolean reset_sig(GtkWidget *w, gpointer data)
{
    struct pr_clock *n = data;

    n->time_elapsed = 0;
    n->time_elapsed_at_start = 0;

    if ( n->start != NULL ) {
        g_date_time_unref(n->start);
    }

    n->start = g_date_time_new_now(n->tz);

    update_clock(n);

    return FALSE;
}


static gboolean setpos_sig(GtkWidget *w, gpointer data)
{
    struct pr_clock *n = data;
    n->pos_reached = n->pos;
    update_clock(n);
    return FALSE;
}


static gboolean start_sig(GtkWidget *w, gpointer data)
{
    struct pr_clock *n = data;

    if ( n->running ) {
        n->running = 0;
        n->time_elapsed_at_start = n->time_elapsed;
        gtk_label_set_text(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(w))),
                           _("Start"));
    } else {
        n->time_elapsed_at_start = n->time_elapsed;
        if ( n->start != NULL ) {
            g_date_time_unref(n->start);
        }
        n->start = g_date_time_new_now(n->tz);
        n->running = 1;
        gtk_label_set_text(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(w))),
                           _("Stop"));
    }

    update_clock(n);

    return FALSE;
}


PRClock *pr_clock_new(PRClock **delete_ptr)
{
    struct pr_clock *n;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *resetbutton;
    GtkWidget *setposbutton;
    GtkWidget *grid;
    GtkWidget *label;

    n = malloc(sizeof(struct pr_clock));
    if ( n == NULL ) return NULL;

    n->delete_ptr = delete_ptr;
    n->tz = g_time_zone_new_local();

    n->window = gtk_window_new();
    gtk_window_set_default_size(GTK_WINDOW(n->window), 600, 150);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_window_set_child(GTK_WINDOW(n->window), vbox);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);

    label = gtk_label_new(_("Length (mins):"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Length (mins):</b>"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_END, NULL);
    gtk_box_append(GTK_BOX(hbox), label);

    n->entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), n->entry);

    n->startbutton = gtk_button_new_with_label(_("Start"));
    gtk_box_append(GTK_BOX(hbox), n->startbutton);

    resetbutton = gtk_button_new_with_label(_("Reset"));
    gtk_box_append(GTK_BOX(hbox), resetbutton);

    setposbutton = gtk_button_new_with_label(_("Set position"));
    gtk_box_append(GTK_BOX(hbox), setposbutton);

    n->da = gtk_drawing_area_new();
    gtk_box_append(GTK_BOX(vbox), n->da);
    gtk_widget_set_vexpand(GTK_WIDGET(n->da), TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(n->da), clock_draw_sig, n, NULL);
    g_signal_connect(G_OBJECT(n->window), "destroy",
                     G_CALLBACK(close_clock_sig), n);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_box_append(GTK_BOX(vbox), grid);
    label = gtk_label_new(_("Time elapsed"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Time elapsed</b>"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    label = gtk_label_new(_("Time remaining"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Time remaining</b>"));
    gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);
    n->status = gtk_label_new("<status>");
    gtk_grid_attach(GTK_GRID(grid), n->status, 2, 0, 1, 1);
    n->elapsed = gtk_label_new("<elapsed>");
    gtk_grid_attach(GTK_GRID(grid), n->elapsed, 0, 1, 1, 1);
    n->remaining = gtk_label_new("<remaining>");
    gtk_grid_attach(GTK_GRID(grid), n->remaining, 1, 1, 1, 1);
    n->wallclock = gtk_label_new("<wall clock>");
    gtk_grid_attach(GTK_GRID(grid), n->wallclock, 2, 1, 1, 1);

    g_signal_connect(G_OBJECT(n->startbutton), "clicked",
                     G_CALLBACK(start_sig), n);
    g_signal_connect(G_OBJECT(resetbutton), "clicked",
                     G_CALLBACK(reset_sig), n);
    g_signal_connect(G_OBJECT(setposbutton), "clicked",
                     G_CALLBACK(setpos_sig), n);
    g_signal_connect(G_OBJECT(n->entry), "changed",
                     G_CALLBACK(set_sig), n);

    n->running = 0;
    n->time_allowed = 0;
    n->time_elapsed = 0;
    n->time_elapsed_at_start = 0;
    n->pos = 0;
    n->pos_reached = 0;
    n->end = 0;
    n->start = NULL;
    update_clock(n);
    n->timer_id = g_timeout_add_seconds(1, update_clock, n);

    gtk_window_set_title(GTK_WINDOW(n->window), _("Presentation clock"));

    gtk_window_present(GTK_WINDOW(n->window));
    return n;
}


void pr_clock_destroy(PRClock *n)
{
    gtk_window_destroy(GTK_WINDOW(n->window));
}
