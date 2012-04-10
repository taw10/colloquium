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

#include "loadsave.h"


struct frame_class
{
	/* Margins of this frame from the parent */
	double   margin_left;
	double   margin_right;
	double   margin_top;
	double   margin_bottom;

	/* Storycode prologue (run through the interpreter before the
	 * main storycode for the frame */
	char    *sc_prologue;
};


struct frame
{
	struct frame_class *cl;

	struct frame      **children;
	int                 n_children;

	char               *sc;  /* Storycode */
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

extern StyleSheet *new_stylesheet();
extern StyleSheet *load_stylesheet(const char *filename);
extern void free_stylesheet(StyleSheet *ss);
extern void default_stylesheet(StyleSheet *ss);

extern int save_stylesheet(StyleSheet *ss, const char *filename);

/* Used during deserialization */
extern struct frame_class *find_slide_template(StyleSheet *ss,
                                               const char *name);
extern struct frame_class *find_frame_class(struct slide_template *st,
                                            const char *name);

extern enum justify str_to_halign(char *halign);
extern enum vert_pos str_to_valign(char *valign);

extern StyleSheet *tree_to_stylesheet(struct ds_node *root);
extern void write_stylesheet(StyleSheet *ss, struct serializer *ser);

#endif	/* STYLESHEET_H */
