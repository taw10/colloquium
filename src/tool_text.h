/*
 * tool_text.h
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

#ifndef TOOL_TEXT_H
#define TOOL_TEXT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

extern struct object *add_text_object(struct slide *s, double x, double y,
                                      struct style *sty);
extern void insert_text(struct object *o, char *t);
extern void handle_text_backspace(struct object *o);
extern void move_cursor_left(struct object *o);
extern void move_cursor_right(struct object *o);
extern void position_caret(struct object *o, double x, double y);


#endif	/* TOOL_TEXT_H */
