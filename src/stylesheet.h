/*
 * stylesheet.h
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

#ifndef STYLESHEET_H
#define STYLESHEET_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


struct frame;
#include "layout.h"
#include "loadsave.h"


struct style
{
	char *name;

	struct layout_parameters lop;

	/* Storycode prologue (run through the interpreter before the
	 * main storycode for the frame */
	char    *sc_prologue;
};


struct slide_template
{
	char                 *name;

	struct frame_class  **frame_classes;
	int                   n_frame_classes;
};


struct _stylesheet
{
	struct style  **styles;
	int             n_styles;
};


typedef struct _stylesheet StyleSheet;
struct presentation;

extern StyleSheet *new_stylesheet();
extern StyleSheet *load_stylesheet(const char *filename);
extern void free_stylesheet(StyleSheet *ss);
extern void default_stylesheet(StyleSheet *ss);

extern struct style *new_style(StyleSheet *ss, const char *name);

extern int save_stylesheet(StyleSheet *ss, const char *filename);

extern struct style *find_style(StyleSheet *ss, const char *name);

extern enum justify str_to_halign(char *halign);
extern enum vert_pos str_to_valign(char *valign);

extern StyleSheet *tree_to_stylesheet(struct ds_node *root);
extern void write_stylesheet(StyleSheet *ss, struct serializer *ser);


#endif	/* STYLESHEET_H */
