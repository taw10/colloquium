/*
 * stylesheet_editor.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>

#include <narrative.h>
#include <stylesheet.h>

#include "stylesheet_editor.h"

struct _sspriv
{
	Stylesheet *stylesheet;
	char *style_name;
};


G_DEFINE_TYPE_WITH_CODE(StylesheetEditor, stylesheet_editor,
                        GTK_TYPE_DIALOG, G_ADD_PRIVATE(StylesheetEditor))


enum selector_column
{
	SEL_COL_FRIENDLY_NAME,
	SEL_COL_PATH
};


static enum gradient id_to_gradtype(const gchar *id)
{
	assert(id != NULL);
	if ( strcmp(id, "flat") == 0 ) return GRAD_NONE;
	if ( strcmp(id, "horiz") == 0 ) return GRAD_HORIZ;
	if ( strcmp(id, "vert") == 0 ) return GRAD_VERT;
	return GRAD_NONE;
}


static enum length_unit id_to_units(const char *id)
{
	if ( strcmp(id, "units") == 0 ) return LENGTH_UNIT;
	if ( strcmp(id, "percent") == 0 ) return LENGTH_FRAC;
	return LENGTH_UNIT;
}


static enum alignment id_to_align(const char *id, int *err)
{
	*err = 0;
	if ( strcmp(id, "left") == 0 ) return ALIGN_LEFT;
	if ( strcmp(id, "center") == 0 ) return ALIGN_CENTER;
	if ( strcmp(id, "right") == 0 ) return ALIGN_RIGHT;
	*err = 1;
	return ALIGN_LEFT;
}


static void set_font_fgcol_align_from_ss(Stylesheet *ss, const char *style_name,
                                         GtkWidget *wfont,
                                         GtkWidget *wfgcol,
                                         GtkWidget *walign)
{
	const char *font;
	struct colour fgcol;
	enum alignment align;

	font = stylesheet_get_font(ss, style_name, &fgcol, &align);
	if ( font != NULL ) {

		GdkRGBA rgba;

		gtk_font_chooser_set_font(GTK_FONT_CHOOSER(wfont), font);

		rgba.red = fgcol.rgba[0];
		rgba.green = fgcol.rgba[1];
		rgba.blue = fgcol.rgba[2];
		rgba.alpha = fgcol.rgba[3];
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wfgcol), &rgba);

		switch ( align ) {

			case ALIGN_LEFT :
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(walign), "left");
			break;

			case ALIGN_CENTER :
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(walign), "center");
			break;

			case ALIGN_RIGHT :
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(walign), "right");
			break;

			default :
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(walign), "left");
			break;

		}

	}
}


static void set_padding_from_ss(Stylesheet *ss, const char *style_name,
                                GtkWidget *wl, GtkWidget *wr,
                                GtkWidget *wt, GtkWidget *wb)
{
	struct length padding[4];

	if ( stylesheet_get_padding(ss, style_name, padding) ) return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wl), padding[0].len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wr), padding[1].len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wt), padding[2].len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wb), padding[3].len);
}


static void set_paraspace_from_ss(Stylesheet *ss, const char *style_name,
                                  GtkWidget *wl, GtkWidget *wr,
                                  GtkWidget *wt, GtkWidget *wb)
{
	struct length paraspace[4];

	if ( stylesheet_get_paraspace(ss, style_name, paraspace) ) return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wl), paraspace[0].len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wr), paraspace[1].len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wt), paraspace[2].len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wb), paraspace[3].len);
}


static void set_geom_from_ss(Stylesheet *ss, const char *style_name,
                             GtkWidget *ww, GtkWidget *wh,
                             GtkWidget *wx, GtkWidget *wy,
                             GtkWidget *wwu, GtkWidget *whu)
{
	struct frame_geom geom;

	if ( stylesheet_get_geometry(ss, style_name, &geom) ) return;

	if ( geom.w.unit == LENGTH_FRAC ) {
		geom.w.len *= 100;
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wwu), "percent");
	} else {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wwu), "units");
	}
	if ( geom.h.unit == LENGTH_FRAC ) {
		geom.h.len *= 100;
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(whu), "percent");
	} else {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(whu), "units");
	}
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ww), geom.w.len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wh), geom.h.len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wx), geom.x.len);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(wy), geom.y.len);
}


static void set_bg_from_ss(Stylesheet *ss, const char *style_name,
                           GtkWidget *wcol, GtkWidget *wcol2, GtkWidget *wgrad)
{
	struct colour bgcol;
	struct colour bgcol2;
	enum gradient bggrad;
	GdkRGBA rgba;

	if ( stylesheet_get_background(ss, style_name, &bggrad, &bgcol, &bgcol2) ) return;

	rgba.red = bgcol.rgba[0];
	rgba.green = bgcol.rgba[1];
	rgba.blue = bgcol.rgba[2];
	rgba.alpha = bgcol.rgba[3];
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba);

	rgba.red = bgcol2.rgba[0];
	rgba.green = bgcol2.rgba[1];
	rgba.blue = bgcol2.rgba[2];
	rgba.alpha = bgcol2.rgba[3];
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol2), &rgba);

	switch ( bggrad ) {

		case GRAD_NONE:
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "flat");
		gtk_widget_set_sensitive(wcol, TRUE);
		gtk_widget_set_sensitive(wcol2, FALSE);
		break;

		case GRAD_HORIZ:
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "horiz");
		gtk_widget_set_sensitive(wcol, TRUE);
		gtk_widget_set_sensitive(wcol2, TRUE);
		break;

		case GRAD_VERT:
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "vert");
		gtk_widget_set_sensitive(wcol, TRUE);
		gtk_widget_set_sensitive(wcol2, TRUE);
		break;

	}

}


static void add_style_to_selector(Stylesheet *ss, const char *path,
                                  GtkTreeStore *ts, GtkTreeIter *parent_iter)
{
	int i, n_substyles;

	n_substyles = stylesheet_get_num_substyles(ss, path);
	for ( i=0; i<n_substyles; i++ ) {

		GtkTreeIter iter;
		const char *name = stylesheet_get_substyle_name(ss, path, i);

		/* Calculate the full path for this style */
		size_t len = strlen(path) + strlen(name) + 2;
		char *new_path = malloc(len);

		if ( path[0] != '\0' ) {
			strcpy(new_path, path);
			strcat(new_path, ".");
		} else {
			new_path[0] = '\0';
		}
		strcat(new_path, name);

		/* Add this style */
		gtk_tree_store_append(ts, &iter, parent_iter);
		gtk_tree_store_set(ts, &iter, SEL_COL_FRIENDLY_NAME,
		                   stylesheet_get_friendly_name(name),
		                   SEL_COL_PATH, new_path, -1);

		/* Add all substyles */
		add_style_to_selector(ss, new_path, ts, &iter);
	}
}


static void set_geom_sensitive(StylesheetEditor *se, gboolean val)
{
	gtk_widget_set_sensitive(se->x, val);
	gtk_widget_set_sensitive(se->y, val);
	gtk_widget_set_sensitive(se->w, val);
	gtk_widget_set_sensitive(se->h, val);
	gtk_widget_set_sensitive(se->w_units, val);
	gtk_widget_set_sensitive(se->h_units, val);
}


static void set_bg_sensitive(StylesheetEditor *se, gboolean val)
{
	gtk_widget_set_sensitive(se->bgcol, val);
	gtk_widget_set_sensitive(se->bgcol2, val);
	gtk_widget_set_sensitive(se->bggrad, val);
}


static void set_padding_sensitive(StylesheetEditor *se, gboolean val)
{
	gtk_widget_set_sensitive(se->padding_l, val);
	gtk_widget_set_sensitive(se->padding_r, val);
	gtk_widget_set_sensitive(se->padding_t, val);
	gtk_widget_set_sensitive(se->padding_b, val);
}


static void set_paraspace_sensitive(StylesheetEditor *se, gboolean val)
{
	gtk_widget_set_sensitive(se->paraspace_l, val);
	gtk_widget_set_sensitive(se->paraspace_r, val);
	gtk_widget_set_sensitive(se->paraspace_t, val);
	gtk_widget_set_sensitive(se->paraspace_b, val);
}


static void set_font_fgcol_align_sensitive(StylesheetEditor *se, gboolean val)
{
	gtk_widget_set_sensitive(se->font, val);
	gtk_widget_set_sensitive(se->fgcol, val);
	gtk_widget_set_sensitive(se->alignment, val);
}


static void set_values_from_presentation(StylesheetEditor *se)
{
	set_geom_from_ss(se->priv->stylesheet, se->priv->style_name,
	                 se->w, se->h, se->x, se->y, se->w_units, se->h_units);

	set_font_fgcol_align_from_ss(se->priv->stylesheet, se->priv->style_name,
	                             se->font, se->fgcol, se->alignment);

	set_bg_from_ss(se->priv->stylesheet, se->priv->style_name,
	               se->bgcol, se->bgcol2, se->bggrad);

	set_padding_from_ss(se->priv->stylesheet, se->priv->style_name,
	                    se->padding_l, se->padding_r, se->padding_t, se->padding_b);

	set_paraspace_from_ss(se->priv->stylesheet, se->priv->style_name,
	                      se->paraspace_l, se->paraspace_r, se->paraspace_t, se->paraspace_b);

	set_geom_sensitive(se, TRUE);
	set_bg_sensitive(se, TRUE);
	set_padding_sensitive(se, TRUE);
	set_font_fgcol_align_sensitive(se, TRUE);
	set_paraspace_sensitive(se, TRUE);
	if ( strncmp(se->priv->style_name, "NARRATIVE", 9) == 0 ) {
		set_geom_sensitive(se, FALSE);
		if ( se->priv->style_name[9] == '.' ) {

			/* Narrative item */
			set_bg_sensitive(se, FALSE);
			set_padding_sensitive(se, FALSE);

		}
	}
	if ( strncmp(se->priv->style_name, "SLIDE", 5) == 0 ) {
		if ( se->priv->style_name[5] == '.' ) {

			/* Slide item */
			set_bg_sensitive(se, FALSE);
			set_padding_sensitive(se, TRUE);

		} else {

			/* Top level "slide" */
			set_geom_sensitive(se, FALSE);
			gtk_widget_set_sensitive(se->w, TRUE);
			gtk_widget_set_sensitive(se->h, TRUE);
			set_padding_sensitive(se, FALSE);
			set_font_fgcol_align_sensitive(se, FALSE);
			set_paraspace_sensitive(se, FALSE);

		}
	}

	if ( strcmp(se->priv->style_name, "SLIDE.TEXT") == 0 ) {
		set_geom_sensitive(se, FALSE);
	}
}


static void element_changed(GtkTreeSelection *sel, StylesheetEditor *se)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *new_path;

	g_free(se->priv->style_name);
	if ( gtk_tree_selection_get_selected(sel, &model, &iter) ) {
		gtk_tree_model_get(model, &iter, SEL_COL_PATH, &new_path, -1);
		se->priv->style_name = new_path;
		set_values_from_presentation(se);
	}
}


static void revert_sig(GtkButton *button, StylesheetEditor *se)
{
	/* FIXME: implement */
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void paraspace_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	struct length paraspace[4];

	paraspace[0].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->paraspace_l));
	paraspace[0].unit = LENGTH_UNIT;
	paraspace[1].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->paraspace_r));
	paraspace[1].unit = LENGTH_UNIT;
	paraspace[2].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->paraspace_t));
	paraspace[2].unit = LENGTH_UNIT;
	paraspace[3].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->paraspace_b));
	paraspace[3].unit = LENGTH_UNIT;

	stylesheet_set_paraspace(se->priv->stylesheet, se->priv->style_name, paraspace);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void padding_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	struct length padding[4];

	padding[0].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->padding_l));
	padding[0].unit = LENGTH_UNIT;
	padding[1].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->padding_r));
	padding[1].unit = LENGTH_UNIT;
	padding[2].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->padding_t));
	padding[2].unit = LENGTH_UNIT;
	padding[3].len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->padding_b));
	padding[3].unit = LENGTH_UNIT;

	stylesheet_set_padding(se->priv->stylesheet, se->priv->style_name, padding);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void alignment_sig(GtkComboBoxText *widget, StylesheetEditor *se)
{
	const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	int err;
	enum alignment align = id_to_align(id, &err);
	if ( !err ) {
		stylesheet_set_alignment(se->priv->stylesheet, se->priv->style_name, align);
		set_values_from_presentation(se);
		g_signal_emit_by_name(se, "changed");
	}
}


static void geometry_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	struct frame_geom new_geom;
	const gchar *uid;

	new_geom.w.len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->w));
	new_geom.h.len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->h));
	new_geom.x.len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->x));
	new_geom.y.len = gtk_spin_button_get_value(GTK_SPIN_BUTTON(se->y));

	uid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(se->w_units));
	new_geom.w.unit = id_to_units(uid);
	uid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(se->h_units));
	new_geom.h.unit = id_to_units(uid);

	new_geom.x.unit = LENGTH_UNIT;
	new_geom.y.unit = LENGTH_UNIT;

	if ( new_geom.w.unit == LENGTH_FRAC ) new_geom.w.len /= 100.0;
	if ( new_geom.h.unit == LENGTH_FRAC ) new_geom.h.len /= 100.0;

	stylesheet_set_geometry(se->priv->stylesheet, se->priv->style_name, new_geom);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void font_sig(GtkFontButton *widget, StylesheetEditor *se)
{
	gchar *font = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(widget));
	stylesheet_set_font(se->priv->stylesheet, se->priv->style_name, font);
	/* Don't free: now owned by stylesheet */

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void fgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	GdkRGBA rgba;
	struct colour col;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &rgba);
	col.rgba[0] = rgba.red;
	col.rgba[1] = rgba.green;
	col.rgba[2] = rgba.blue;
	col.rgba[3] = rgba.alpha;
	col.hexcode = 0;
	stylesheet_set_fgcol(se->priv->stylesheet, se->priv->style_name, col);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void bg_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	enum gradient g;
	const gchar *id;
	GdkRGBA rgba;
	struct colour bgcol, bgcol2;

	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(se->bggrad));
	g = id_to_gradtype(id);

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(se->bgcol), &rgba);
	if ( rgba.alpha < 0.000001 ) rgba.alpha = 0.0;
	bgcol.rgba[0] = rgba.red;
	bgcol.rgba[1] = rgba.green;
	bgcol.rgba[2] = rgba.blue;
	bgcol.rgba[3] = rgba.alpha;
	bgcol.hexcode = 0;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(se->bgcol2), &rgba);
	if ( rgba.alpha < 0.000001 ) rgba.alpha = 0.0;
	bgcol2.rgba[0] = rgba.red;
	bgcol2.rgba[1] = rgba.green;
	bgcol2.rgba[2] = rgba.blue;
	bgcol2.rgba[3] = rgba.alpha;
	bgcol2.hexcode = 0;

	stylesheet_set_background(se->priv->stylesheet, se->priv->style_name, g, bgcol, bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void stylesheet_editor_finalize(GObject *obj)
{
	G_OBJECT_CLASS(stylesheet_editor_parent_class)->finalize(obj);
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

	gobject_class->finalize = stylesheet_editor_finalize;

	SE_BIND_CHILD(paraspace_l, paraspace_sig);
	SE_BIND_CHILD(paraspace_r, paraspace_sig);
	SE_BIND_CHILD(paraspace_t, paraspace_sig);
	SE_BIND_CHILD(paraspace_b, paraspace_sig);
	SE_BIND_CHILD(padding_l, padding_sig);
	SE_BIND_CHILD(padding_r, padding_sig);
	SE_BIND_CHILD(padding_t, padding_sig);
	SE_BIND_CHILD(padding_b, padding_sig);
	SE_BIND_CHILD(font, font_sig);
	SE_BIND_CHILD(fgcol, fgcol_sig);
	SE_BIND_CHILD(bgcol, bg_sig);
	SE_BIND_CHILD(bgcol2, bg_sig);
	SE_BIND_CHILD(bggrad, bg_sig);
	SE_BIND_CHILD(alignment, alignment_sig);
	SE_BIND_CHILD(w, geometry_sig);
	SE_BIND_CHILD(h, geometry_sig);
	SE_BIND_CHILD(x, geometry_sig);
	SE_BIND_CHILD(y, geometry_sig);
	SE_BIND_CHILD(w_units, geometry_sig);
	SE_BIND_CHILD(h_units, geometry_sig);

	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor, selector);
	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor, element_tree);

	gtk_widget_class_bind_template_callback(widget_class, revert_sig);

	g_signal_new("changed", COLLOQUIUM_TYPE_STYLESHEET_EDITOR,
	             G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}


StylesheetEditor *stylesheet_editor_new(Stylesheet *ss)
{
	StylesheetEditor *se;
	GtkTreeSelection *sel;

	se = g_object_new(COLLOQUIUM_TYPE_STYLESHEET_EDITOR, NULL);
	if ( se == NULL ) return NULL;

	se->priv->stylesheet = ss;
	se->priv->style_name = NULL;

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Element", renderer,
	                                                  "text", SEL_COL_FRIENDLY_NAME,
	                                                  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(se->selector), column);

	gtk_tree_store_clear(se->element_tree);
	add_style_to_selector(se->priv->stylesheet, "", se->element_tree, NULL);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(se->selector));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(sel), "changed", G_CALLBACK(element_changed), se);

	return se;
}

