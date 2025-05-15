/*
 * slide_window.h
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

#ifndef SLIDEWINDOW_H
#define SLIDEWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _gtkslidewindow SlideWindow;
typedef struct _gtkslidewindowclass SlideWindowClass;

#include "narrative.h"
#include "narrative_window.h"

#define GTK_TYPE_SLIDE_WINDOW (gtk_slide_window_get_type())

#define GTK_SLIDE_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                 GTK_TYPE_SLIDE_WINDOW, SlideWindow))

#define GTK_IS_SLIDE_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                    GTK_TYPE_SLIDE_WINDOW))

#define GTK_SLIDE_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((obj), \
                                         GTK_TYPE_SLIDE_WINDOW, SlideWindowClass))

#define GTK_IS_SLIDE_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                            GTK_TYPE_SLIDE_WINDOW))

#define GTK_SLIDE_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                           GTK_TYPE_SLIDE_WINDOW, SlideWindowClass))

struct _gtkslidewindow
{
    GtkApplicationWindow parent_instance;

    /*< private >*/
    Narrative           *n;
    Slide               *slide;
    GtkWidget           *sv;
    NarrativeWindow     *parent;
    int                  laser_on;
};

struct _gtkslidewindowclass
{
    GtkApplicationWindowClass parent_class;
};

extern GType gtk_slide_window_get_type(void);

extern SlideWindow *slide_window_new(Narrative *n,
                                     Slide *slide,
                                     NarrativeWindow *parent,
                                     GApplication *papp);

extern void slide_window_update(SlideWindow *sw);
extern void slide_window_update_titlebar(SlideWindow *sw);
extern void slide_window_set_slide(SlideWindow *sw, Slide *s);
extern void slide_window_set_laser(SlideWindow *sw, double x, double y);
extern void slide_window_set_laser_off(SlideWindow *sw);

#endif	/* SLIDEWINDOW_H */
