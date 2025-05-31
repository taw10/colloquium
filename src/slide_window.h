/*
 * slide_window.h
 *
 * Copyright © 2013-2025 Thomas White <taw@bitwiz.me.uk>
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

#ifndef SLIDEWINDOW_H
#define SLIDEWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _slidewindow SlideWindow;
typedef struct _slidewindowclass SlideWindowClass;

#include "narrative.h"
#include "narrative_window.h"

#define COLLOQUIUM_TYPE_SLIDE_WINDOW (colloquium_slide_window_get_type())

#define COLLOQUIUM_SLIDE_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                      COLLOQUIUM_TYPE_SLIDE_WINDOW, SlideWindow))

struct _slidewindow
{
    GtkApplicationWindow parent_instance;

    /*< private >*/
    Narrative           *n;
    Slide               *slide;
    GtkWidget           *sv;
    NarrativeWindow     *parent;
    int                  laser_on;
};

struct _slidewindowclass
{
    GtkApplicationWindowClass parent_class;
};

extern GType colloquium_slide_window_get_type(void);

extern SlideWindow *slide_window_new(Narrative *n,
                                     Slide *slide,
                                     NarrativeWindow *parent,
                                     GApplication *papp);

extern void slide_window_update(SlideWindow *sw);
extern void slide_window_update_titlebar(SlideWindow *sw);
extern void slide_window_set_slide(SlideWindow *sw, Slide *s);
extern void slide_window_set_laser(SlideWindow *sw, double x, double y);
extern void slide_window_set_laser_off(SlideWindow *sw);
extern void slide_window_fullscreen_on_monitor(SlideWindow *sw, GdkMonitor *mon);

#endif	/* SLIDEWINDOW_H */
