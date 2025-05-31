/*
 * narrative_window.h
 *
 * Copyright © 2014-2025 Thomas White <taw@bitwiz.me.uk>
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

#ifndef NARRATIVE_WINDOW_H
#define NARRATIVE_WINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _narrativewindow NarrativeWindow;
typedef struct _narrativewindowclass NarrativeWindowClass;

#define COLLOQUIUM_TYPE_NARRATIVE_WINDOW (colloquium_narrative_window_get_type())

#define COLLOQUIUM_NARRATIVE_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                          COLLOQUIUM_TYPE_NARRATIVE_WINDOW, NarrativeWindow))

#include "pr_clock.h"
#include "slide_window.h"
#include "narrative.h"
#include "slide_sorter.h"

struct _narrativewindow
{
    GtkApplicationWindow parent_instance;

    /*< private >*/
    GtkWidget           *nv;
    GApplication        *app;
    Narrative           *n;
    GFile               *file;
    PRClock             *pr_clock;
    SlideWindow         *slidewindows[16];
    int                  n_slidewindows;
    GtkWidget           *timing_ruler;
    SlideSorter         *slide_sorter;
    GtkWidget           *toolbar;
    int                  presenting;
    GtkWidget           *presenting_label;
    Slide               *presenting_slide;
    GSettings           *settings;
};


struct _narrativewindowclass
{
    GtkApplicationWindowClass parent_class;
};


extern GType colloquium_narrative_window_get_type(void);

extern NarrativeWindow *narrative_window_new(Narrative *n,
                                             GFile *file,
                                             GApplication *app);

extern char *narrative_window_get_filename(NarrativeWindow *nw);

extern void slide_window_closed_sig(GtkWidget *sw, NarrativeWindow *nw);

#endif	/* NARRATIVE_WINDOW_H */
