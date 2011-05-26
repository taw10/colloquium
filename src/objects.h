/*
 * presentation.h
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

#ifndef OBJECTS_H
#define OBJECTS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


enum objtype
{
	TEXT,
};


struct object
{
	enum objtype   type;
	struct slide  *parent;

	/* Position of corner of object */
	double         x;
	double         y;

	/* Side of rectangular bounding box of object */
	double         bb_width;
	double         bb_height;

	int            empty;

	/* For type TEXT */
	char          *text;
	size_t         text_len;
	int            insertion_point;
	PangoLayout   *layout;
	PangoFontDescription *fontdesc;
};


extern struct object *add_text_object(struct slide *s, double x, double y);
extern void insert_text(struct object *o, char *t);
extern void handle_text_backspace(struct object *o);
extern void move_cursor_left(struct object *o);
extern void move_cursor_right(struct object *o);

extern void delete_object(struct object *o);


#endif	/* OBJECTS_H */
