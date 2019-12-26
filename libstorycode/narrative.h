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

#include "slide.h"
#include "storycode.h"
#include "imagestore.h"

extern Narrative *narrative_new(void);
extern void narrative_free(Narrative *n);
extern void narrative_add_empty_item(Narrative *n);

extern void narrative_add_stylesheet(Narrative *n, Stylesheet *ss);
extern Stylesheet *narrative_get_stylesheet(Narrative *n);

extern const char *narrative_get_language(Narrative *n);
extern ImageStore *narrative_get_imagestore(Narrative *n);

extern Narrative *narrative_load(GFile *file);
extern int narrative_save(Narrative *n, GFile *file);

extern void narrative_set_unsaved(Narrative *n);
extern int narrative_get_unsaved(Narrative *n);

extern int narrative_item_is_text(Narrative *n, int item);

extern void narrative_add_text(Narrative *n, struct text_run *runs, int n_runs);

extern void narrative_add_bp(Narrative *n, struct text_run *runs, int n_runs);

extern void narrative_add_prestitle(Narrative *n, struct text_run *runs, int n_runs);

extern void narrative_add_slide(Narrative *n, Slide *slide);
extern void narrative_add_eop(Narrative *n);
extern void narrative_insert_slide(Narrative *n, Slide *slide, int pos);
extern void narrative_delete_block(Narrative *n, int i1, size_t o1,
                                                 int i2, size_t o2);
extern void narrative_split_item(Narrative *n, int i1, size_t o1);
extern int narrative_get_num_items(Narrative *n);
extern int narrative_get_num_items_to_eop(Narrative *n);
extern int narrative_get_num_slides(Narrative *n);
extern Slide *narrative_get_slide(Narrative *n, int para);
extern Slide *narrative_get_slide_by_number(Narrative *n, int pos);
extern int narrative_get_slide_number_for_para(Narrative *n, int para);
extern int narrative_get_slide_number_for_slide(Narrative *n, Slide *s);

extern char *narrative_range_as_text(Narrative *n, int p1, size_t o1, int p2, size_t o2);
extern char *narrative_range_as_storycode(Narrative *n, int p1, size_t o1, int p2, size_t o2);

extern void narrative_debug(Narrative *n);

#endif /* NARRATIVE_H */
