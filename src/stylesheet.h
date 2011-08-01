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


struct text_style
{
	char   *name;
	char   *font;
	char   *colour;
	double  alpha;
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


struct layout_element
{
	char              *name;
	struct text_style *text_style;

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
};


typedef struct _stylesheetwindow StylesheetWindow;
typedef struct _stylesheet StyleSheet;
struct presentation;

extern StylesheetWindow *open_stylesheet(struct presentation *p);

extern StyleSheet *new_stylesheet();
extern StyleSheet *load_stylesheet(const char *filename);

#endif	/* STYLESHEET_H */
