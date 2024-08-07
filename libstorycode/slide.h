/*
 * slide.h
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

#ifndef SLIDE_H
#define SLIDE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>

typedef struct _slide Slide;
typedef struct _slideitem SlideItem;

#include "stylesheet.h"
#include "storycode.h"

extern Slide *slide_new(void);
extern void slide_free(Slide *s);

extern SlideItem *slide_add_item(Slide *s, SlideItem *item);
extern void slide_delete_item(Slide *s, SlideItem *item);

extern int slide_set_logical_size(Slide *s, double w, double h);
extern int slide_get_logical_size(Slide *s, Stylesheet *ss, double *w, double *h);


/* Constructors for slide items */
extern SlideItem *slide_item_image(char *filename, struct frame_geom geom);
extern SlideItem *slide_item_text(struct text_run **paras, int *n_runs, int n_paras,
                                 struct frame_geom geom, enum alignment alignment);
extern SlideItem *slide_item_footer(void);
extern SlideItem *slide_item_slidetitle(struct text_run **paras, int *n_runs, int n_paras);
extern SlideItem *slide_item_prestitle(struct text_run **paras, int *n_runs, int n_paras);


/* Slide items */
extern void slide_item_get_geom(SlideItem *item, Stylesheet *ss,
                                double *x, double *y, double *w, double *h,
                                double slide_w, double slide_h);

extern void slide_item_get_padding(SlideItem *item, Stylesheet *ss,
                                   double *l, double *r, double *t, double *b,
                                   double slide_w, double slide_h);

extern void slide_item_split_text_paragraph(SlideItem *item, int para, size_t off);
extern void slide_item_delete_text(SlideItem *item, int p1, size_t o1, int p2, size_t o2);

extern void slide_item_to_top(Slide *s, SlideItem *item);
extern void slide_item_to_bottom(Slide *s, SlideItem *item);

/* For debugging, not really part of API */
extern void describe_slide(Slide *s);

#endif /* SLIDE_H */
