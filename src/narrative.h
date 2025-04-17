/*
 * narrative.h
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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

#ifndef NARRATIVE_H
#define NARRATIVE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

typedef struct _narrative Narrative;

#include "slide.h"

struct time_mark
{
    double minutes;
    double y;
};


struct _narrative
{
    int saved;
    const char *language;
    GtkTextBuffer *textbuf;

    /* NB These are NOT necessarily in "presenting order"! */
    int n_slides;
    int max_slides;
    Slide **slides;

    struct time_mark *time_marks;
    int n_time_marks;
};


enum narrative_item_type
{
    NARRATIVE_ITEM_TEXT,
    NARRATIVE_ITEM_SEGSTART,
    NARRATIVE_ITEM_SEGEND,
    NARRATIVE_ITEM_PRESTITLE,
    NARRATIVE_ITEM_SLIDE,
    NARRATIVE_ITEM_BP,
    NARRATIVE_ITEM_EOP,
};

extern Narrative *narrative_new(void);
extern void narrative_free(Narrative *n);

extern Narrative *narrative_load(GFile *file);
extern int narrative_save(Narrative *n, GFile *file);

extern void insert_slide_anchor(GtkTextBuffer *buf, Slide *slide, GtkTextIter start, int newline);
extern void narrative_update_timing(GtkTextView *nv, Narrative *n);
extern Slide *narrative_get_slide(Narrative *n, int para);

#endif /* NARRATIVE_H */
