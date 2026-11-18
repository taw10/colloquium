/*
 * slide_title_window.h
 *
 * Copyright © 2026 Thomas White <taw@bitwiz.me.uk>
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

#ifndef SLIDETITLEWINDOW_H
#define SLIDETITLEWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _slidetitlewindow SlideTitleWindow;
typedef struct _slidetitlewindowclass SlideTitleWindowClass;

#include "slide.h"

#define COLLOQUIUM_SLIDE_TITLE_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                      COLLOQUIUM_TYPE_SLIDE_TITLE_WINDOW, SlideTitleWindow))

#define COLLOQUIUM_TYPE_SLIDE_TITLE_WINDOW (colloquium_slide_title_window_get_type())

struct _slidetitlewindow
{
    GtkWindow parent_instance;

    /*< private >*/
    GtkWidget *entry;
    Slide *s;
};

struct _slidetitlewindowclass
{
    GtkWindowClass parent_class;
};

extern SlideTitleWindow *slide_title_window_new(Slide *s);

#endif	/* SLIDETITLEWINDOW_H */
