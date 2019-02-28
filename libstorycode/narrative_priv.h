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

#include "storycode.h"


enum narrative_item_type
{
	NARRATIVE_ITEM_TEXT,
	NARRATIVE_ITEM_SLIDE,
	NARRATIVE_ITEM_BP,
};


struct narrative_item
{
	enum narrative_item_type type;

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
};


struct _narrative
{
	int n_items;
	struct narrative_item *items;
	double w;
	double total_h;
};


#endif /* NARRATIVE_PRIV_H */
