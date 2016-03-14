/*
 * frame.h
 *
 * Copyright © 2013-2015 Thomas White <taw@bitwiz.org.uk>
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

#ifndef FRAME_H
#define FRAME_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pango/pango.h>
#include <cairo.h>

#include "sc_parse.h"


typedef enum
{
	UNITS_SLIDE,
	UNITS_FRAC
} LengthUnits;


typedef enum
{
	GRAD_NONE,
	GRAD_HORIZ,
	GRAD_VERT
} GradientType;


struct frame
{
	struct frame            **children;
	int                       num_children;
	int                       max_children;

	SCBlock                  *scblocks;
	struct boxvec            *boxes;  /* The unwrapped boxes */
	int                       visited;

	int                       n_paragraphs;
	struct boxvec           **paragraphs;
	int                      *paragraph_start_lines;

	int                       n_lines;
	int                       max_lines;
	struct wrap_line         *lines;

	/* The font which will be used by default for this frame */
	PangoFontDescription     *fontdesc;

	/* The rectangle allocated to this frame, determined by the renderer */
	double                    x;
	double                    y;
	double                    w;
	double                    h;

	double                    pad_t;
	double                    pad_b;
	double                    pad_l;
	double                    pad_r;

	/* Background properties for this frame */
	double                    bgcol[4];
	double                    bgcol2[4];
	GradientType              grad;

	/* True if this frame should be deleted on the next mouse click */
	int                       empty;

	/* True if the aspect ratio of this frame should be maintained */
	int                       is_image;

	/* True if this frame can be resized and moved */
	int                       resizable;

	/* True if wrapping failed for this frame */
	int                       trouble;
};


extern struct frame *frame_new(void);
extern void frame_free(struct frame *fr);
extern struct frame *add_subframe(struct frame *fr);
extern void show_hierarchy(struct frame *fr, const char *t);
extern void delete_subframe(struct frame *top, struct frame *fr);
extern struct frame *find_frame_with_scblocks(struct frame *top,
                                              SCBlock *scblocks);

#endif	/* FRAME_H */
