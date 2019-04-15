/*
 * stylesheet_editor.h
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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

#ifndef STYLESHEET_EDITOR_H
#define STYLESHEET_EDITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <narrative.h>

#define COLLOQUIUM_TYPE_STYLESHEET_EDITOR (stylesheet_editor_get_type())

#define COLLOQUIUM_STYLESHEET_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                             COLLOQUIUM_TYPE_STYLESHEET_EDITOR, \
                                             StylesheetEditor))

#define COLLOQUIUM_IS_STYLESHEET_EDITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                                COLLOQUIUM_TYPE_STYLESHEET_EDITOR))

#define COLLOQUIUM_STYLESHEET_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((obj), \
                                                     COLLOQUIUM_TYPE_STYLESHEET_EDITOR, \
                                                     StylesheetEditorClass))

#define COLLOQUIUM_IS_STYLESHEET_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                                        COLLOQUIUM_TYPE_STYLESHEET_EDITOR))

#define COLLOQUIUM_STYLESHEET_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                                        COLLOQUIUM_TYPE_STYLESHEET_EDITOR, \
                                                        StylesheetEditorClass))


typedef struct _sspriv StylesheetEditorPrivate;

struct _stylesheeteditor
{
	GtkDialog parent_instance;
	GtkWidget *selector;
	GtkWidget *paraspace_l;
	GtkWidget *paraspace_r;
	GtkWidget *paraspace_t;
	GtkWidget *paraspace_b;
	GtkWidget *padding_l;
	GtkWidget *padding_r;
	GtkWidget *padding_t;
	GtkWidget *padding_b;
	GtkWidget *font;
	GtkWidget *fgcol;
	GtkWidget *bgcol;
	GtkWidget *bgcol2;
	GtkWidget *bggrad;
	GtkWidget *alignment;
	GtkWidget *w;
	GtkWidget *h;
	GtkWidget *x;
	GtkWidget *y;
	GtkWidget *w_units;
	GtkWidget *h_units;
	StylesheetEditorPrivate *priv;
};

struct _stylesheeteditorclass
{
	GtkDialogClass parent_class;
};

typedef struct _stylesheeteditor StylesheetEditor;
typedef struct _stylesheeteditorclass StylesheetEditorClass;

extern StylesheetEditor *stylesheet_editor_new(Stylesheet *ss);

#endif	/* STYLESHEET_EDITOR_H */
