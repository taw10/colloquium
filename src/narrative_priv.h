/*
 * narrative_priv.h
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

#ifndef NARRATIVE_PRIV_H
#define NARRATIVE_PRIV_H

#ifdef HAVE_PANGO
#include <pango/pangocairo.h>
#endif

#include <gtk/gtk.h>

#include "slide.h"


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


struct narrative_item
{
    enum narrative_item_type type;

    /* Space around the thing (PangoLayout, slide, marker etc) */
    double space_l;
    double space_r;
    double space_t;
    double space_b;

    /* Size of the thing (PangoLayout, slide, marker etc) */
    double obj_w;
    double obj_h;

    /* Total height is obj_h + space_t + space_b.
     * obj_w + space_l + space_r might be less than width of rendering surface */

    /* For TEXT, BP, PRESTITLE */
    int n_runs;
    struct text_run *runs;

    /* For SLIDE */
    Slide *slide;
    GtkTextChildAnchor *anchor;
    int selected;  /* Whether or not this item should be given a "selected" highlight */

    double estd_duration;  /* Estimated duration in minutes, based on word count */
};


struct _narrative
{
    int saved;
    const char *language;

    GtkTextBuffer *textbuf;

    int n_items;
    struct narrative_item *items;

    double w;
    double space_l;
    double space_r;
    double space_t;
    double space_b;
};

extern int narrative_which_run(struct narrative_item *item, size_t item_offs, size_t *run_offs);

extern void update_timing(struct narrative_item *item);


#endif /* NARRATIVE_PRIV_H */
