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

static void set_values_from_presentation(StylesheetEditor *se);

struct _sspriv
{
	struct presentation *p;
};


static void find_replace(SCBlock *parent, const char *find, const char *seti)
{
	SCBlock *bl = sc_block_child(parent);

	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);
		if ( (name != NULL) && (strcmp(name, find)==0) ) {
			printf("replaced '%s' with '%s'\n", sc_block_options(bl),
			       seti);
			sc_block_set_options(bl, strdup(seti));
			return;
		}

		bl = sc_block_next(bl);

	}
}


static void set_ss(struct presentation *p, const char *style_name,
                   const char *find, const char *seti)
{
	const char *name;
	SCBlock *bl;

	/* If no stylesheet yet, create one now */
	if ( p->stylesheet == NULL ) {
		p->stylesheet = sc_parse("\\stylesheet");
		if ( p->stylesheet == NULL ) {
			fprintf(stderr, "WARNING: Couldn't create stylesheet\n");
			return;
		}
		sc_block_append_p(p->stylesheet, p->scblocks);
		p->scblocks = p->stylesheet;
	}
	bl = p->stylesheet;

	name = sc_block_name(bl);
	if ( (name != NULL) && (strcmp(name, "stylesheet")==0) ) {
		bl = sc_block_child(bl);
	}

	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);
		const char *options = sc_block_options(bl);
		if ( (name != NULL) && (strcmp(name, "style")==0)
		  && (strcmp(options, style_name)==0) )
		{
			printf("found style %s\n", options);
			find_replace(bl, find, seti);
			return;
		}

		bl = sc_block_next(bl);

	}

	/* Not found -> add style */
	bl = sc_block_append_inside(p->stylesheet, strdup("style"), strdup(style_name), NULL);
	sc_block_append_inside(bl, strdup(find), strdup(seti), NULL);
}


static void revert_sig(GtkButton *button, StylesheetEditor *widget)
{
	printf("click revert!\n");
}


static GradientType id_to_gradtype(const gchar *id)
{
	if ( strcmp(id, "flat") == 0 ) return GRAD_NONE;
	if ( strcmp(id, "horiz") == 0 ) return GRAD_HORIZ;
	if ( strcmp(id, "vert") == 0 ) return GRAD_VERT;
	return GRAD_NONE;
}


static void set_font(GtkFontButton *widget, StylesheetEditor *se,
                     const char *style_name)
{
	const gchar *font;
	font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));
	set_ss(se->priv->p, style_name, "font", font);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void set_col(GtkColorButton *widget, StylesheetEditor *se,
                    const char *style_name, const char *col_name)
{
	GdkRGBA rgba;
	gchar *col;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &rgba);
	col = gdk_rgba_to_string(&rgba);
	set_ss(se->priv->p, style_name, col_name, col);
	g_free(col);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void set_bggrad(GtkComboBox *widget, StylesheetEditor *se,
                       const char *style_name)
{
	const gchar *id;
	GradientType grad;
	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	grad = id_to_gradtype(id);
	//set_ss(se->priv->p, style_name, "bggrad", grad);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_font_sig(GtkFontButton *widget, StylesheetEditor *se)
{
	set_font(widget, se, "narrative");
}


static void narrative_fgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "narrative", "fgcol");
}


static void narrative_bgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "narrative", "bgcol");
}


static void narrative_bgcol2_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "narrative", "bgcol2");
}


static void narrative_bggrad_sig(GtkComboBox *widget, StylesheetEditor *se)
{
	set_bggrad(widget, se, "narrative");
}


static void slide_bgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "slide", "bgcol");
}


static void slide_bgcol2_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "slide", "bgcol2");
}


static void slide_bggrad_sig(GtkComboBox *widget, StylesheetEditor *se)
{
	set_bggrad(widget, se, "slide");
}


static void frame_font_sig(GtkFontButton *widget, StylesheetEditor *se)
{
	set_font(widget, se, "frame");
}


static void frame_fgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "frame", "fgcol");
}


static void frame_bgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "frame", "bgcol");
}


static void frame_bgcol2_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "frame", "bgcol2");
}


static void frame_bggrad_sig(GtkComboBox *widget, StylesheetEditor *se)
{
	set_bggrad(widget, se, "frame");
}


static void stylesheet_editor_init(StylesheetEditor *se)
{
	se->priv = G_TYPE_INSTANCE_GET_PRIVATE(se, COLLOQUIUM_TYPE_STYLESHEET_EDITOR,
	                                       StylesheetEditorPrivate);
	gtk_widget_init_template(GTK_WIDGET(se));
}


#define SE_BIND_CHILD(a, b) \
	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor, a); \
	gtk_widget_class_bind_template_callback(widget_class, b);

void stylesheet_editor_class_init(StylesheetEditorClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class,
	                                    "/uk/me/bitwiz/Colloquium/stylesheeteditor.ui");

	g_type_class_add_private(gobject_class, sizeof(StylesheetEditorPrivate));


	/* Narrative style */
	SE_BIND_CHILD(narrative_style_font, narrative_font_sig);
	SE_BIND_CHILD(narrative_style_fgcol, narrative_fgcol_sig);
	SE_BIND_CHILD(narrative_style_bgcol, narrative_bgcol_sig);
	SE_BIND_CHILD(narrative_style_bgcol2, narrative_bgcol2_sig);
	SE_BIND_CHILD(narrative_style_bggrad, narrative_bggrad_sig);

	/* Slide style */
	SE_BIND_CHILD(slide_style_bgcol, slide_bgcol_sig);
	SE_BIND_CHILD(slide_style_bgcol2, slide_bgcol2_sig);
	SE_BIND_CHILD(slide_style_bggrad, slide_bggrad_sig);

	/* Slide->frame style */
	SE_BIND_CHILD(frame_style_font, frame_font_sig);
	SE_BIND_CHILD(frame_style_fgcol, frame_fgcol_sig);
	SE_BIND_CHILD(frame_style_bgcol, frame_bgcol_sig);
	SE_BIND_CHILD(frame_style_bgcol2, frame_bgcol2_sig);
	SE_BIND_CHILD(frame_style_bggrad, frame_bggrad_sig);

	gtk_widget_class_bind_template_callback(widget_class, revert_sig);

	g_signal_new("changed", COLLOQUIUM_TYPE_STYLESHEET_EDITOR,
	             G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}


static void set_from_interp_col(double *col, GtkWidget *w)
{
	GdkRGBA rgba;

	rgba.red = col[0];
	rgba.green = col[1];
	rgba.blue = col[2];
	rgba.alpha = col[3];
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &rgba);
}


static void set_from_interp_bggrad(SCInterpreter *scin, GtkWidget *w)
{
	GradientType grad;
	const gchar *id;

	grad = sc_interp_get_bggrad(scin);

	switch ( grad ) {
		case GRAD_NONE : id = "flat"; break;
		case GRAD_HORIZ : id = "horiz"; break;
		case GRAD_VERT : id = "vert"; break;
	}

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(w), id);
}


static void set_from_interp_font(SCInterpreter *scin, GtkWidget *w)
{
	char *fontname;
	PangoFontDescription *fontdesc;

	fontdesc = sc_interp_get_fontdesc(scin);
	fontname = pango_font_description_to_string(fontdesc);
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(w), fontname);
	g_free(fontname);
}


static void set_values_from_presentation(StylesheetEditor *se)
{
	SCInterpreter *scin;

	scin = sc_interp_new(NULL, NULL, NULL, NULL);
	sc_interp_run_stylesheet(scin, se->priv->p->stylesheet);  /* NULL stylesheet is OK */

	/* Narrative style */
	sc_interp_save(scin);
	sc_interp_run_style(scin, "narrative");
	set_from_interp_font(scin, se->narrative_style_font);
	set_from_interp_col(sc_interp_get_fgcol(scin), se->narrative_style_fgcol);
	set_from_interp_col(sc_interp_get_bgcol(scin), se->narrative_style_bgcol);
	set_from_interp_col(sc_interp_get_bgcol2(scin), se->narrative_style_bgcol2);
	set_from_interp_bggrad(scin, se->narrative_style_bggrad);
	sc_interp_restore(scin);

	/* Slide style */
	sc_interp_save(scin);
	sc_interp_run_style(scin, "silde");
	set_from_interp_col(sc_interp_get_bgcol(scin), se->slide_style_bgcol);
	set_from_interp_col(sc_interp_get_bgcol2(scin), se->slide_style_bgcol2);
	set_from_interp_bggrad(scin, se->slide_style_bggrad);
	sc_interp_restore(scin);

	/* Slide->Frame style */
	sc_interp_save(scin);
	sc_interp_run_style(scin, "frame");
	set_from_interp_font(scin, se->frame_style_font);
	set_from_interp_col(sc_interp_get_fgcol(scin), se->frame_style_fgcol);
	set_from_interp_col(sc_interp_get_bgcol(scin), se->frame_style_bgcol);
	set_from_interp_col(sc_interp_get_bgcol2(scin), se->frame_style_bgcol2);
	set_from_interp_bggrad(scin, se->frame_style_bggrad);
	sc_interp_restore(scin);

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

