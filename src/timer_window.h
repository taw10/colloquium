/*
 * timer_window.h
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

#ifndef COLLOQUIUM_TIMER_WINDOW_H
#define COLLOQUIUM_TIMER_WINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

typedef struct _colloquiumtimerwindow TimerWindow;
typedef struct _colloquiumtimerwindowclass TimerWindowClass;

#define COLLOQUIUM_TYPE_TIMER_WINDOW (colloquium_timer_window_get_type())

#define COLLOQUIUM_TIMER_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                     COLLOQUIUM_TYPE_TIMER_WINDOW, TimerWindow))

struct _colloquiumtimerwindow
{
    GtkWindow parent_instance;

    /*< private >*/
    GtkWidget *entry;
    GtkWidget *qentry;
    GtkWidget *startbutton;
    GtkWidget *da;
    GtkWidget *elapsed;
    GtkWidget *remaining;
    GtkWidget *status;
    GtkWidget *bar;
    Timer *timer;
    guint timer_id;
};

struct _colloquiumtimerwindowclass
{
    GtkWindowClass parent_class;
};

extern GType colloquium_timer_window_get_type(void);

extern TimerWindow *colloquium_timer_window_new(Timer *timer);

#endif  /* COLLOQUIUM_TIMER_WINDOW_H */
