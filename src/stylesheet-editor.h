/*
 * stylesheet-editor.h
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

#ifndef STYLESHEET_EDITOR_H
#define STYLESHEET_EDITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


typedef struct _stylesheetwindow StylesheetWindow;

extern StylesheetWindow *open_stylesheet(struct presentation *p);

#endif	/* STYLESHEET_EDITOR_H */
