/*
 * timer_window.c
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
#include "timer_window.h"

G_DEFINE_FINAL_TYPE(TimerWindow, colloquium_timer_window, GTK_TYPE_WINDOW)


static void colloquium_timer_window_class_init(TimerWindowClass *klass)
{
}


static void colloquium_timer_window_init(TimerWindow *e)
{
}


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
    TimerWindow *n = data;
    double time_remaining;
    double time_used;
    double progress_time;
    char *tmp;

    time_used = colloquium_timer_get_elapsed_main_time(n->timer);
    time_remaining = colloquium_timer_get_main_time(n->timer)
                      - colloquium_timer_get_elapsed_main_time(n->timer);
    progress_time = colloquium_timer_get_main_time(n->timer)
                      * colloquium_timer_get_max_progress_fraction(n->timer);

    tmp = format_span(time_used);
    gtk_label_set_text(GTK_LABEL(n->elapsed), tmp);
    free(tmp);

    tmp = format_span(time_remaining);
    gtk_label_set_text(GTK_LABEL(n->remaining), tmp);
    free(tmp);

    tmp = format_span_nice(progress_time - time_used);
    gtk_label_set_text(GTK_LABEL(n->status), tmp);
    free(tmp);

    return TRUE;
}


static void close_clock_sig(GtkWidget *w, TimerWindow *n)
{
    g_source_remove(n->timer_id);
    g_signal_handlers_disconnect_by_data(G_OBJECT(n->timer), n);
}


static void set_sig(GtkEditable *w, TimerWindow *n)
{
    gchar *tstr;
    double t1, t2;
    char *check;

    tstr = gtk_editable_get_chars(GTK_EDITABLE(n->entry), 0, -1);
    t1 = 60.0 * strtod(tstr, &check);
    if ( check == tstr ) {
        fprintf(stderr, "Invalid time '%s'\n", tstr);
    }
    g_free(tstr);

    tstr = gtk_editable_get_chars(GTK_EDITABLE(n->qentry), 0, -1);
    t2 = 60.0 * strtod(tstr, &check);
    if ( check == tstr ) {
        fprintf(stderr, "Invalid time '%s'\n", tstr);
    }
    g_free(tstr);

    colloquium_timer_set_main_time(n->timer, t1);
    colloquium_timer_set_discussion_time(n->timer, t2);

    update_clock(n);
}


static gboolean reset_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_reset(n->timer);
    gtk_label_set_text(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(n->startbutton))),
                       _("Start"));
    update_clock(n);
    return FALSE;
}


static gboolean setpos_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_reset_max_progress(n->timer);
    update_clock(n);
    return FALSE;
}


static gboolean start_sig(GtkWidget *w, TimerWindow *n)
{
    if ( colloquium_timer_get_running(n->timer) ) {
        colloquium_timer_pause(n->timer);
        gtk_label_set_text(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(w))),
                           _("Start"));
    } else {
        colloquium_timer_start(n->timer, TIMER_MAIN);
        gtk_label_set_text(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(w))),
                           _("Stop"));
    }

    update_clock(n);

    return FALSE;
}


TimerWindow *colloquium_timer_window_new(Timer *timer)
{
    TimerWindow *n;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *resetbutton;
    GtkWidget *setposbutton;
    GtkWidget *grid;
    GtkWidget *label;

    n = g_object_new(COLLOQUIUM_TYPE_TIMER_WINDOW, NULL);
    gtk_window_set_default_size(GTK_WINDOW(n), 600, 150);
    n->timer = timer;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_window_set_child(GTK_WINDOW(n), vbox);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_box_set_spacing(GTK_BOX(hbox), 4);

    label = gtk_label_new(_("Main time (mins):"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Main time (mins):</b>"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_END, NULL);
    gtk_box_append(GTK_BOX(hbox), label);

    n->entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), n->entry);

    label = gtk_label_new(_("Discussion time (mins):"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Discussion time (mins):</b>"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_END, NULL);
    gtk_box_append(GTK_BOX(hbox), label);

    n->qentry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), n->qentry);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_box_set_spacing(GTK_BOX(hbox), 4);

    n->startbutton = gtk_button_new_with_label(_("Start"));
    gtk_box_append(GTK_BOX(hbox), n->startbutton);

    resetbutton = gtk_button_new_with_label(_("Reset"));
    gtk_box_append(GTK_BOX(hbox), resetbutton);

    setposbutton = gtk_button_new_with_label(_("Set position"));
    gtk_box_append(GTK_BOX(hbox), setposbutton);

    n->bar = colloquium_timer_bar_new(timer);
    gtk_box_append(GTK_BOX(vbox), n->bar);
    gtk_widget_set_vexpand(GTK_WIDGET(n->bar), TRUE);

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

    g_signal_connect(G_OBJECT(n->startbutton), "clicked", G_CALLBACK(start_sig), n);
    g_signal_connect(G_OBJECT(resetbutton), "clicked", G_CALLBACK(reset_sig), n);
    g_signal_connect(G_OBJECT(setposbutton), "clicked", G_CALLBACK(setpos_sig), n);
    g_signal_connect(G_OBJECT(n->entry), "changed", G_CALLBACK(set_sig), n);
    g_signal_connect(G_OBJECT(n->qentry), "changed", G_CALLBACK(set_sig), n);

    n->timer_id = g_timeout_add_seconds(1, update_clock, n);

    gtk_window_set_title(GTK_WINDOW(n), _("Presentation clock"));
    g_signal_connect(G_OBJECT(n), "destroy", G_CALLBACK(close_clock_sig), n);

    gtk_window_present(GTK_WINDOW(n));
    return n;
}
