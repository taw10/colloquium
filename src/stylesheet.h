/*
 * stylesheet.h
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2011 Thomas White <taw@bitwiz.org.uk>
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

#ifndef STYLESHEET_H
#define STYLESHEET_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


struct frame_class
{
	char *name;

	/* Margins of this frame from the parent */
	double   margin_l;
	double   margin_r;
	double   margin_t;
	double   margin_b;

	/* Padding between this frame and any children */
	double   pad_l;
	double   pad_r;
	double   pad_t;
	double   pad_b;

	/* Storycode prologue (run through the interpreter before the
	 * main storycode for the frame */
	char    *sc_prologue;
};


enum bgblocktype
{
	BGBLOCK_SOLID,
	BGBLOCK_GRADIENT_X,
	BGBLOCK_GRADIENT_Y,
	BGBLOCK_GRADIENT_CIRCULAR,
	BGBLOCK_IMAGE,
};


struct bgblock
{
	enum bgblocktype type;
	double           min_x;
	double           max_x;
	double           min_y;
	double           max_y;

	char            *colour1;
	double           alpha1;
	char            *colour2;
	double           alpha2;

	struct image    *image;
	GdkPixbuf       *scaled_pb;
	int              scaled_w;
	int              scaled_h;
};


struct slide_template
{
	char                 *name;

	struct frame_class  **frame_classes;
	int                   n_frame_classes;

	struct bgblock      **bgblocks;
	int                   n_bgblocks;
};


typedef struct _stylesheet StyleSheet;
struct presentation;

#endif	/* STYLESHEET_H */
