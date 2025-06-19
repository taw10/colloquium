/*
 * timer_bar.h
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

#ifndef COLLOQUIUM_TIMER_BAR_H
#define COLLOQUIUM_TIMER_BAR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

typedef struct _colloquiumtimerbar TimerBar;
typedef struct _colloquiumtimerbarclass TimerBarClass;

#define COLLOQUIUM_TYPE_TIMER_BAR (colloquium_timer_bar_get_type())

#define COLLOQUIUM_TIMER_BAR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                  COLLOQUIUM_TYPE_TIMER_BAR, TimerBar))

struct _colloquiumtimerbar
{
    GtkWidget            parent_instance;

    /*< private >*/
    Timer *timer;
    guint timer_id;
};

struct _colloquiumtimerbarclass
{
    GtkWidgetClass parent_class;
};

extern GType colloquium_timer_bar_get_type(void);

extern GtkWidget *colloquium_timer_bar_new(Timer *t);

#endif  /* COLLOQUIUM_TIMER_BAR_H */
