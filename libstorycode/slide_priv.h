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


struct slide_text_paragraph
{
	struct text_run *runs;
	int n_runs;
#ifdef HAVE_PANGO
	PangoLayout *layout;
#else
	void *layout;
#endif
};


struct _slideitem
{
	enum slide_item_type type;

	/* For TEXT, SLIDETITLE, PRESTITLE */
	int n_paras;
	struct slide_text_paragraph *paras;
	enum alignment align;

	/* For IMAGE */
	char *filename;

	/* For TEXT and IMAGE */
	struct frame_geom geom;
};


struct _slide
{
	double logical_w;
	double logical_h;
	int n_items;
	SlideItem *items;
};

#endif /* SLIDE_PRIV_H */
