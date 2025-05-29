/*
 * prefswindow.h
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

#ifndef PREFSWINDOW_H
#define PREFSWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _prefswindow PrefsWindow;
typedef struct _prefswindowclass PrefsWindowClass;

#define COLLOQUIUM_PREFS_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                      COLLOQUIUM_TYPE_PREFS_WINDOW, PrefsWindow))

#define COLLOQUIUM_TYPE_PREFS_WINDOW (colloquium_prefs_window_get_type())

struct _prefswindow
{
    GtkWindow parent_instance;

    /*< private >*/
};

struct _prefswindowclass
{
    GtkWindowClass parent_class;
};

extern PrefsWindow *prefs_window_new();

#endif	/* PREFSWINDOW_H */
