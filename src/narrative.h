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

#include <gio/gio.h>

typedef struct _narrative Narrative;

enum text_run_type
{
    TEXT_RUN_NORMAL,
    TEXT_RUN_BOLD,
    TEXT_RUN_ITALIC,
    TEXT_RUN_UNDERLINE,
};


struct text_run
{
    enum text_run_type type;
    char *text;
};


#include "slide.h"

extern Narrative *narrative_new(void);
extern void narrative_free(Narrative *n);
extern void narrative_add_empty_item(Narrative *n);

extern Narrative *narrative_load(GFile *file);
extern int narrative_save(Narrative *n, GFile *file);

extern void narrative_set_unsaved(Narrative *n);
extern int narrative_get_unsaved(Narrative *n);

extern void narrative_insert_text(Narrative *n, int pos, struct text_run *runs, int n_runs);
extern void narrative_insert_bp(Narrative *n, int pos, struct text_run *runs, int n_runs);
extern void narrative_insert_segstart(Narrative *n, int pos, struct text_run *runs, int n_runs);
extern void narrative_insert_prestitle(Narrative *n, int pos, struct text_run *runs, int n_runs);
extern void narrative_insert_segend(Narrative *n, int pos);
extern void narrative_insert_slide(Narrative *n, int pos, Slide *slide);
extern void narrative_insert_eop(Narrative *n, int pos);

extern void narrative_delete_block(Narrative *n, int i1, size_t o1,
                                                 int i2, size_t o2);
extern void narrative_split_item(Narrative *n, int i1, size_t o1);
extern void narrative_delete_item(Narrative *n, int del);
extern int narrative_get_num_items(Narrative *n);
extern int narrative_get_num_items_to_eop(Narrative *n);
extern int narrative_get_num_slides(Narrative *n);
extern Slide *narrative_get_slide(Narrative *n, int para);
extern Slide *narrative_get_slide_by_number(Narrative *n, int pos);
extern int narrative_get_slide_number_for_slide(Narrative *n, Slide *s);
extern void narrative_get_first_slide_size(Narrative *n, double *w, double *h);

extern double narrative_find_time_pos(Narrative *n, double minutes);

#endif /* NARRATIVE_H */
