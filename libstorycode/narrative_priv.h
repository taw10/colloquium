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

#include "imagestore.h"
#include "storycode.h"
#include "slide.h"


enum narrative_item_type
{
	NARRATIVE_ITEM_TEXT,
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
	char *text;
	enum alignment align;
#ifdef HAVE_PANGO
	PangoLayout *layout;
#else
	void *layout;
#endif

	/* For SLIDE */
	Slide *slide;
#ifdef HAVE_CAIRO
	cairo_surface_t *slide_thumbnail;
#else
	void *slide_thumbnail;
#endif
	int selected;  /* Whether or not this item should be given a "selected" highlight */
};


struct _narrative
{
	Stylesheet *stylesheet;
	ImageStore *imagestore;
	int saved;
	const char *language;

	int n_items;
	struct narrative_item *items;

	double w;
	double space_l;
	double space_r;
	double space_t;
	double space_b;
};


#endif /* NARRATIVE_PRIV_H */
