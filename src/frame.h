/*
 * frame.h
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

#ifndef FRAME_H
#define FRAME_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pango/pango.h>
#include <cairo.h>


typedef enum
{
	DIR_NONE,
	DIR_UL,
	DIR_U,
	DIR_UR,
	DIR_R,
	DIR_DR,
	DIR_D,
	DIR_DL,
	DIR_L
} Direction;


typedef enum
{
	UNITS_SLIDE,
	UNITS_FRAC
} LengthUnits;


struct layout_parameters
{
	double margin_l;
	double margin_r;
	double margin_t;
	double margin_b;

	double pad_l;
	double pad_r;
	double pad_t;
	double pad_b;

	double x;
	double y;

	double w;
	LengthUnits w_units;
	double h;
	LengthUnits h_units;
};


struct frame
{
	struct frame            **children;
	int                       num_children;
	int                       max_children;

	char                     *sc;  /* Storycode */
	size_t                    sc_len;  /* Space allocated for sc */

	cairo_surface_t          *contents;

	int                       n_lines;
	int                       max_lines;
	struct wrap_line         *lines;

	size_t                    pos;

	struct layout_parameters  lop;
	struct style             *style;  /* Non-NULL if 'lop' came from SS */

	/* The rectangle allocated to this frame, determined by the renderer */
	double                    x;
	double                    y;
	double                    w;
	double                    h;

	PangoLayout              *pl;

	/* True if this frame should be deleted on the next mouse click */
	int                       empty;
};


extern struct frame *frame_new(void);
extern struct frame *add_subframe(struct frame *fr);
extern struct frame *sc_unpack(const char *sc);

#endif	/* FRAME_H */
