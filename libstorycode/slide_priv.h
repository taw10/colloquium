/*
 * slide_priv.h
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

#ifndef SLIDE_PRIV_H
#define SLIDE_PRIV_H

#ifdef HAVE_PANGO
#include <pango/pangocairo.h>
#endif

#include "storycode.h"

enum slide_item_type
{
	SLIDE_ITEM_TEXT,
	SLIDE_ITEM_IMAGE,
	SLIDE_ITEM_FOOTER,
	SLIDE_ITEM_SLIDETITLE,
	SLIDE_ITEM_PRESTITLE,
};


struct slide_item
{
	enum slide_item_type type;

	/* For TEXT, SLIDETITLE, PRESTITLE */
	char **paragraphs;
	int n_paras;
	enum alignment align;
#ifdef HAVE_PANGO
	PangoLayout **layouts;
#endif

	/* For IMAGE */
	char *filename;

	/* For TEXT and IMAGE */
	struct frame_geom geom;
	int resizable;
};


struct _slide
{
	double logical_w;
	double logical_h;
	int n_items;
	struct slide_item *items;
};

extern void slide_item_get_geom(struct slide_item *item, Stylesheet *ss,
                                double *x, double *y, double *w, double *h,
                                double slide_w, double slide_h);

extern void slide_item_get_padding(struct slide_item *item, Stylesheet *ss,
                                   double *l, double *r, double *t, double *b,
                                   double slide_w, double slide_h);

#endif /* SLIDE_PRIV_H */
