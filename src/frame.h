/*
 * frame.h
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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

	Direction grav;

	double min_w_u;
	double min_h_u;
	double min_w_frac;
	double min_h_frac;
};


struct frame
{
	struct frame            **children;
	int                       num_children;
	int                       max_children;

	char                     *sc;  /* Storycode */

	cairo_surface_t          *contents;

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
