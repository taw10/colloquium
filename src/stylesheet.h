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


enum object_role
{
	S_ROLE_NONE        = 0,
	S_ROLE_SLIDENUMBER = 1,
	S_ROLE_PTITLE      = 2,  /* Presentation title on slide */
	S_ROLE_PTITLE_REF  = 3,  /* Reference to actual title */
	S_ROLE_PAUTHOR     = 4,
	S_ROLE_PAUTHOR_REF = 5,
	S_ROLE_PDATE       = 6,
	S_ROLE_PDATE_REF   = 7,
	NUM_S_ROLES
};


enum justify
{
	J_LEFT   = 0,
	J_CENTER = 1,
	J_RIGHT  = 2,
};


enum vert_pos
{
	V_TOP     = 0,
	V_CENTER  = 1,
	V_BOTTOM  = 2,
};


struct style
{
	char              *name;
	enum object_role   role;

	double             margin_left;
	double             margin_right;
	double             margin_top;
	double             margin_bottom;
	double             max_width;
	int                use_max_width;

	enum justify       halign;
	enum vert_pos      valign;

	double             offset_x;
	double             offset_y;

	char              *font;
	char              *colour;
	double             alpha;
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


struct _stylesheet
{
	struct style  **styles;
	int             n_styles;

	struct bgblock *bgblocks;
	int             n_bgblocks;

	/* Background stuff */
};


typedef struct _stylesheetwindow StylesheetWindow;
typedef struct _stylesheet StyleSheet;
struct presentation;

extern StylesheetWindow *open_stylesheet(struct presentation *p);

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
