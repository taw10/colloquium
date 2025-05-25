/*
 * slide_sorter.h
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

#ifndef SLIDESORTER_H
#define SLIDESORTER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _slidesorter SlideSorter;
typedef struct _slidesorterclass SlideSorterClass;

#include "narrative_window.h"

#define COLLOQUIUM_SLIDE_SORTER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                      COLLOQUIUM_TYPE_SLIDE_SORTER, SlideSorter))

#define COLLOQUIUM_TYPE_SLIDE_SORTER (colloquium_slide_sorter_get_type())

struct _slidesorter
{
    GtkWindow parent_instance;

    /*< private >*/
    NarrativeWindow     *parent;
    GtkWidget           *flowbox;
    GSList              *source_files;
};

struct _slidesorterclass
{
    GtkWindowClass parent_class;
};

extern SlideSorter *slide_sorter_new(NarrativeWindow *parent);

#endif	/* SLIDESORTER_H */
