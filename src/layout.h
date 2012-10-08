/*
 * layout.h
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

#ifndef LAYOUT_H
#define LAYOUT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


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

	double min_w;
	double min_h;
};


/* Calculate layout for frame (and all its children) based on size */
extern void layout_frame(struct frame *fr, double w, double h,
                         PangoContext *pc);


#endif	/* LAYOUT_H */
