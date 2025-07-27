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
    int mins, sec;
    char *s;

    if ( n < 0 ) {
        s = "-";
        n = -n;
    } else {
        s = "";
    }

    sec = n % 60;
    mins = ((n-sec) % (60*60))/60;

    snprintf(tmp, 31, "%s%02i:%02i", s, mins, sec);

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
    char *tmp1, *tmp2;
    char tmp[1024];

    time_used = colloquium_timer_get_elapsed_main_time(n->timer);
    time_remaining = colloquium_timer_get_main_time(n->timer)
                      - colloquium_timer_get_elapsed_main_time(n->timer);
    progress_time = colloquium_timer_get_main_time(n->timer)
                      * colloquium_timer_get_max_progress_fraction(n->timer);

    tmp1 = format_span_nice(progress_time - time_used);
    tmp2 = format_span(time_remaining);
    snprintf(tmp, 1023, "%s.  %s remaining", tmp1, tmp2);
    gtk_label_set_text(GTK_LABEL(n->remaining), tmp);
    snprintf(tmp, 1023, "<b>%s</b>.  %s remaining", tmp1, tmp2);
    gtk_label_set_markup(GTK_LABEL(n->remaining), tmp);
    free(tmp1);
    free(tmp2);

    tmp1 = format_span(time_used);
    snprintf(tmp, 1023, "%s elapsed", tmp1);
    gtk_label_set_text(GTK_LABEL(n->elapsed), tmp);
    free(tmp1);

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
}


static void set_buttons(TimerWindow *n)
{
    if ( colloquium_timer_get_running(n->timer) ) {

        switch ( colloquium_timer_get_running_mode(n->timer) ) {

            case TIMER_MAIN:
            gtk_widget_set_sensitive(n->startbutton, FALSE);
            gtk_widget_set_sensitive(n->discussionbutton, TRUE);
            gtk_widget_set_sensitive(n->pausebutton, TRUE);
            break;

            case TIMER_DISCUSSION:
            gtk_widget_set_sensitive(n->startbutton, TRUE);
            gtk_widget_set_sensitive(n->discussionbutton, FALSE);
            gtk_widget_set_sensitive(n->pausebutton, TRUE);
            break;

        }
    } else {
        gtk_widget_set_sensitive(n->startbutton, TRUE);
        gtk_widget_set_sensitive(n->discussionbutton, TRUE);
        gtk_widget_set_sensitive(n->pausebutton, FALSE);
    }
}


static gboolean reset_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_reset(n->timer);
    return FALSE;
}


static gboolean setpos_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_reset_max_progress(n->timer);
    return FALSE;
}


static gboolean pause_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_pause(n->timer);
    return FALSE;
}


static gboolean start_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_start(n->timer, TIMER_MAIN);
    return FALSE;
}


static gboolean discussion_sig(GtkWidget *w, TimerWindow *n)
{
    colloquium_timer_start(n->timer, TIMER_DISCUSSION);
    return FALSE;
}


static void clock_event_sig(Timer *t, TimerWindow *n)
{
    update_clock(n);
    set_buttons(n);
}


static GtkWidget *timer_buttons(TimerWindow *n)
{
    GtkWidget *grid;
    GtkWidget *resetbutton;
    GtkWidget *setposbutton;

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    n->startbutton = gtk_button_new_with_label(_("Start"));
    gtk_widget_set_name(n->startbutton, "timer-start-button");
    gtk_grid_attach(GTK_GRID(grid), n->startbutton, 0, 0, 1, 1);

    n->pausebutton = gtk_button_new_with_label(_("Pause"));
    gtk_widget_set_name(n->pausebutton, "timer-stop-button");
    gtk_grid_attach(GTK_GRID(grid), n->pausebutton, 0, 1, 1, 1);

    n->discussionbutton = gtk_button_new_with_label(_("Discussion"));
    gtk_widget_set_name(n->discussionbutton, "timer-discussion-button");
    gtk_grid_attach(GTK_GRID(grid), n->discussionbutton, 0, 2, 1, 1);

    resetbutton = gtk_button_new_with_label(_("Reset"));
    gtk_widget_set_name(resetbutton, "timer-reset-button");
    gtk_grid_attach(GTK_GRID(grid), resetbutton, 1, 0, 1, 1);

    setposbutton = gtk_button_new_with_label(_("Set position"));
    gtk_grid_attach(GTK_GRID(grid), setposbutton, 1, 1, 1, 1);

    g_signal_connect(G_OBJECT(n->startbutton), "clicked", G_CALLBACK(start_sig), n);
    g_signal_connect(G_OBJECT(n->pausebutton), "clicked", G_CALLBACK(pause_sig), n);
    g_signal_connect(G_OBJECT(n->discussionbutton), "clicked", G_CALLBACK(discussion_sig), n);
    g_signal_connect(G_OBJECT(resetbutton), "clicked", G_CALLBACK(reset_sig), n);
    g_signal_connect(G_OBJECT(setposbutton), "clicked", G_CALLBACK(setpos_sig), n);

    return grid;
}


static GtkWidget *timer_entries(TimerWindow *n)
{
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *label;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_box_set_spacing(GTK_BOX(hbox), 4);

    label = gtk_label_new(_("Main time (mins):"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Main time (mins):</b>"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_END, NULL);
    gtk_box_append(GTK_BOX(hbox), label);

    n->entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), n->entry);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_box_set_spacing(GTK_BOX(hbox), 4);

    label = gtk_label_new(_("Discussion time (mins):"));
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Discussion time (mins):</b>"));
    g_object_set(G_OBJECT(label), "halign", GTK_ALIGN_END, NULL);
    gtk_box_append(GTK_BOX(hbox), label);

    n->qentry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), n->qentry);

    g_signal_connect(G_OBJECT(n->entry), "changed", G_CALLBACK(set_sig), n);
    g_signal_connect(G_OBJECT(n->qentry), "changed", G_CALLBACK(set_sig), n);

    return vbox;
}


TimerWindow *colloquium_timer_window_new(Timer *timer)
{
    TimerWindow *n;
    GtkWidget *vbox;
    GtkWidget *hbox;

    n = g_object_new(COLLOQUIUM_TYPE_TIMER_WINDOW, NULL);
    gtk_window_set_default_size(GTK_WINDOW(n), 600, 150);
    n->timer = timer;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_window_set_child(GTK_WINDOW(n), vbox);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);

    n->bar = colloquium_timer_bar_new(timer);
    gtk_box_append(GTK_BOX(vbox), n->bar);
    gtk_widget_set_vexpand(GTK_WIDGET(n->bar), TRUE);

    n->remaining = gtk_label_new("<remaining>");
    gtk_box_append(GTK_BOX(vbox), n->remaining);
    gtk_widget_set_name(n->remaining, "timer-remaining");
    n->elapsed = gtk_label_new(_("Timer is not running"));
    gtk_box_append(GTK_BOX(vbox), n->elapsed);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_box_set_spacing(GTK_BOX(hbox), 8);

    gtk_box_append(GTK_BOX(hbox), timer_buttons(n));
    gtk_box_append(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(hbox), timer_entries(n));

    n->timer_id = g_timeout_add_seconds(1, update_clock, n);
    g_signal_connect(G_OBJECT(n->timer), "clock-event", G_CALLBACK(clock_event_sig), n);
    update_clock(n);
    set_buttons(n);

    gtk_window_set_title(GTK_WINDOW(n), _("Presentation clock"));
    g_signal_connect(G_OBJECT(n), "destroy", G_CALLBACK(close_clock_sig), n);

    gtk_window_present(GTK_WINDOW(n));
    return n;
}
