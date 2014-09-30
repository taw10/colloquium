/*
 * sc_editor.h
 *
 * Copyright © 2014 Thomas White <taw@bitwiz.org.uk>
 *
 * This file is part of Colloquium.
 *
 * Colloquium is free software: you can redistribute it and/or modify
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

#ifndef SC_EDITOR_H
#define SC_EDITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>


struct presentation;
typedef struct _sceditor SCEditor;

extern void sc_editor_set_slide(SCEditor *e, struct slide *s);
extern GtkWidget *sc_editor_get_widget(SCEditor *e);
extern SCEditor *sc_editor_new(struct presentation *p);
extern void sc_editor_set_size(SCEditor *e, int w, int h);

#endif	/* SC_EDITOR_H */
