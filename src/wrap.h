/*
 * wrap.h
 *
 * Text wrapping, hyphenation, justification and shaping
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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

#ifndef WRAP_H
#define WRAP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "frame.h"


enum wrap_box_type
{
	WRAP_BOX_NOTHING,
	WRAP_BOX_PANGO,
	WRAP_BOX_IMAGE
};


/* Possible types of space following a wrap box */
enum wrap_box_space
{
	WRAP_SPACE_INTERWORD,  /* Normal inter-word space */
	WRAP_SPACE_NONE,       /* Box ends with an explicit hyphen or an SC
	                        * block boundary */
	WRAP_SPACE_EOP         /* End of paragraph */
	/* TODO: Space between sentences etc */
};


/* A wrap box is a run of content - could be text, an image or so one - that is
 * one logical unit as far as Colloquium is concerned.  It might consist of
 * multiple units, for example, in Pango's mind. */
struct wrap_box
{
	enum wrap_box_type type;

	/* Pango units */
	int width;
	int height;
	int ascent;

	enum wrap_box_space space;  /* Type of "space" following box */

	/* For type == WRAP_BOX_PANGO */
	PangoGlyphString *glyphs;
	PangoFont *font;
	char *text;
	double col[4];  /* rgba colour */

	/* For type == WRAP_BOX_IMAGE */
	/* Nothing yet */
};


struct wrap_line
{
	int width;   /* Pango units */
	int height;  /* Pango units */
	int ascent;  /* Pango units */

	int n_boxes;
	int max_boxes;
	struct wrap_box *boxes;

	size_t sc_offset;
};


extern int wrap_contents(struct frame *fr, PangoContext *pc);

extern void get_cursor_pos(struct frame *fr, size_t pos,
                           double *xposd, double *yposd, double *line_height);


#endif	/* WRAP_H */
