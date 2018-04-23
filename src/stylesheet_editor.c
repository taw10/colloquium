/*
 * stylesheet_editor.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>

#include "stylesheet_editor.h"
#include "presentation.h"
#include "sc_interp.h"


G_DEFINE_TYPE_WITH_CODE(StylesheetEditor, stylesheet_editor,
                        GTK_TYPE_DIALOG, NULL)

struct _sspriv
{

	struct presentation *p;
};


static void revert_sig(GtkButton *button, StylesheetEditor *widget)
{
	printf("click revert!\n");
}


static void stylesheet_editor_init(StylesheetEditor *se)
{
	se->priv = G_TYPE_INSTANCE_GET_PRIVATE(se, COLLOQUIUM_TYPE_STYLESHEET_EDITOR,
	                                       StylesheetEditorPrivate);
	gtk_widget_init_template(GTK_WIDGET(se));
}


void stylesheet_editor_class_init(StylesheetEditorClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class,
	                                    "/uk/me/bitwiz/Colloquium/stylesheeteditor.ui");

	g_type_class_add_private(gobject_class, sizeof(StylesheetEditorPrivate));

	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor,
	                                     default_style_ss);
	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor,
	                                     default_style_font);
	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor,
	                                     default_style_fgcol);

	gtk_widget_class_bind_template_callback(widget_class, revert_sig);
}


static void set_values_from_presentation(StylesheetEditor *se)
{
	SCInterpreter *scin;
	char *fontname;
	PangoFontDescription *fontdesc;
	double *col;
	GdkRGBA rgba;

	scin = sc_interp_new(NULL, NULL, NULL, NULL);
	sc_interp_run_stylesheet(scin, se->priv->p->stylesheet);  /* NULL stylesheet is OK */

	/* Default style */
	fontdesc = sc_interp_get_fontdesc(scin);
	fontname = pango_font_description_to_string(fontdesc);
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(se->default_style_font), fontname);
	g_free(fontname);

	col = sc_interp_get_fgcol(scin);
	rgba.red = col[0];
	rgba.green = col[1];
	rgba.blue = col[2];
	rgba.alpha = col[3];
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(se->default_style_fgcol), &rgba);

	sc_interp_destroy(scin);
}


StylesheetEditor *stylesheet_editor_new(struct presentation *p)
{
	StylesheetEditor *se;

	se = g_object_new(COLLOQUIUM_TYPE_STYLESHEET_EDITOR, NULL);
	if ( se == NULL ) return NULL;

	se->priv->p = p;
	set_values_from_presentation(se);

	return se;
}

