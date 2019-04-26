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


G_DEFINE_TYPE_WITH_CODE(StylesheetEditor, stylesheet_editor,
                        GTK_TYPE_DIALOG, NULL)


struct _sspriv
{
	Stylesheet *stylesheet;
	const char *style_name;
};


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
	/* FIXME: units */
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
	/* FIXME: units */
}


static void set_geom_from_ss(Stylesheet *ss, const char *style_name,
                             GtkWidget *ww, GtkWidget *wh,
                             GtkWidget *wx, GtkWidget *wy,
                             GtkWidget *wwu, GtkWidget *whu)
{
	struct frame_geom geom;

	if ( stylesheet_get_geometry(ss, style_name, &geom) ) return;

	if ( geom.x.unit == LENGTH_FRAC ) {
		geom.w.len *= 100;
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wwu), "percent");
	} else {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wwu), "units");
	}
	if ( geom.y.unit == LENGTH_FRAC ) {
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


static void add_style(Stylesheet *ss, const char *path,
                      GtkTreeStore *ts, GtkTreeIter *parent_iter)
{
	int i, n_substyles;

	n_substyles = stylesheet_get_num_substyles(ss, path);
	for ( i=0; i<n_substyles; i++ ) {

		GtkTreeIter iter;
		const char *name = stylesheet_get_substyle_name(ss, path, i);

		/* Add this style */
		gtk_tree_store_append(ts, &iter, parent_iter);
		gtk_tree_store_set(ts, &iter, 0, name, -1);

		/* Add all substyles */
		size_t len = strlen(path) + strlen(name) + 2;
		char *new_path = malloc(len);
		strcat(new_path, path);
		strcat(new_path, ".");
		strcat(new_path, name);
		add_style(ss, new_path, ts, &iter);
	}
}


static void set_values_from_presentation(StylesheetEditor *se)
{
	gtk_tree_store_clear(se->element_tree);
	add_style(se->priv->stylesheet, "", se->element_tree, NULL);

	set_geom_from_ss(se->priv->stylesheet, se->priv->style_name,
	                 se->w, se->h, se->x, se->y, se->w_units, se->h_units);

	set_padding_from_ss(se->priv->stylesheet, se->priv->style_name,
	                    se->padding_l, se->padding_r, se->padding_t, se->padding_b);

	set_paraspace_from_ss(se->priv->stylesheet, se->priv->style_name,
	                      se->paraspace_l, se->paraspace_r, se->paraspace_t, se->paraspace_b);

	set_font_fgcol_align_from_ss(se->priv->stylesheet, se->priv->style_name,
	                             se->font, se->fgcol, se->alignment);
	set_bg_from_ss(se->priv->stylesheet, se->priv->style_name,
	               se->bgcol, se->bgcol2, se->bggrad);
}


static enum gradient id_to_gradtype(const gchar *id)
{
	assert(id != NULL);
	if ( strcmp(id, "flat") == 0 ) return GRAD_NONE;
	if ( strcmp(id, "horiz") == 0 ) return GRAD_HORIZ;
	if ( strcmp(id, "vert") == 0 ) return GRAD_VERT;
	return GRAD_NONE;
}


static void update_bg(Stylesheet *ss, const char *style_name,
                      GtkWidget *bggradw, GtkWidget *col1w, GtkWidget *col2w)
{
	enum gradient g;
	const gchar *id;
	GdkRGBA rgba;
	struct colour bgcol, bgcol2;

	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(bggradw));
	g = id_to_gradtype(id);

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(col1w), &rgba);
	if ( rgba.alpha < 0.000001 ) rgba.alpha = 0.0;
	bgcol.rgba[0] = rgba.red;
	bgcol.rgba[1] = rgba.green;
	bgcol.rgba[2] = rgba.blue;
	bgcol.rgba[3] = rgba.alpha;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(col2w), &rgba);
	if ( rgba.alpha < 0.000001 ) rgba.alpha = 0.0;
	bgcol2.rgba[0] = rgba.red;
	bgcol2.rgba[1] = rgba.green;
	bgcol2.rgba[2] = rgba.blue;
	bgcol2.rgba[3] = rgba.alpha;

	stylesheet_set_background(ss, style_name, g, bgcol, bgcol2);
}


static char units_id_to_char(const char *id)
{
	if ( strcmp(id, "units") == 0 ) return 'u';
	if ( strcmp(id, "percent") == 0 ) return 'f';
	return 'u';
}


static void update_ss_dims(Stylesheet *ss, const char *style_name,
                           const char *key, GtkWidget *ww, GtkWidget *wh,
                           GtkWidget *wx, GtkWidget *wy,
                           GtkWidget *wwu, GtkWidget *whu)
{
#if 0
	float w, h, x, y;
	char w_units, h_units;
	const gchar *uid;
	char tmp[256];

	w = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ww));
	h = gtk_spin_button_get_value(GTK_SPIN_BUTTON(wh));
	x = gtk_spin_button_get_value(GTK_SPIN_BUTTON(wx));
	y = gtk_spin_button_get_value(GTK_SPIN_BUTTON(wy));
	uid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(wwu));
	w_units = units_id_to_char(uid);
	uid = gtk_combo_box_get_active_id(GTK_COMBO_BOX(whu));
	h_units = units_id_to_char(uid);

	if ( w_units == 'f' ) w /= 100.0;
	if ( h_units == 'f' ) h /= 100.0;

	if ( snprintf(tmp, 256, "%.2f%cx%.2f%c+%.0f+%0.f",
		      w, w_units, h, h_units, x, y) >= 256 )
	{
		fprintf(stderr, "Spacing too long\n");
	} else {
		stylesheet_set(p->stylesheet, style_name, key, tmp);
	}
#endif
}


static void revert_sig(GtkButton *button, StylesheetEditor *se)
{
	/* FIXME: implement */
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void set_font(GtkFontButton *widget, StylesheetEditor *se,
                     const char *style_name)
{
#if 0
	const gchar *font;
	font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));

	stylesheet_set(se->priv->p->stylesheet, style_name, "font", font);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
#endif
}


static void set_col(GtkColorButton *widget, StylesheetEditor *se,
                    const char *style_name, const char *col_name)
{
#if 0
	GdkRGBA rgba;
	gchar *col;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &rgba);
	col = gdk_rgba_to_string(&rgba);
	stylesheet_set(se->priv->p->stylesheet, style_name, "fgcol", col);
	g_free(col);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
#endif
}


static void paraspace_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
#if 0
	update_spacing(se->priv->p, se->priv->furniture, "pad",
	               se->furniture_padding_l,
	               se->furniture_padding_r,
	               se->furniture_padding_t,
	               se->furniture_padding_b);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
#endif
}


static void padding_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
#if 0
	update_spacing(se->priv->p, se->priv->furniture, "pad",
	               se->furniture_padding_l,
	               se->furniture_padding_r,
	               se->furniture_padding_t,
	               se->furniture_padding_b);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
#endif
}


static void alignment_sig(GtkComboBoxText *widget, StylesheetEditor *se)
{
#if 0
	const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	stylesheet_set(se->priv->p->stylesheet, se->priv->furniture,
	               "alignment", id);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
#endif
}


static void dims_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
#if 0
	update_ss_dims(se->priv->p, se->priv->furniture, "geometry",
	               se->furniture_w, se->furniture_h,
	               se->furniture_x, se->furniture_y,
	               se->furniture_w_units, se->furniture_h_units);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
#endif
}


static void font_sig(GtkFontButton *widget, StylesheetEditor *se)
{
}


static void fgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
}


static void bg_sig(GtkColorButton *widget, StylesheetEditor *se)
{
}


static void selector_change_sig(GtkComboBoxText *widget, StylesheetEditor *se)
{
	//se->priv->el = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	//set_furniture(se, se->priv->el);
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

	g_type_class_add_private(gobject_class, sizeof(StylesheetEditorPrivate));
	gobject_class->finalize = stylesheet_editor_finalize;

	/* Furniture */
	SE_BIND_CHILD(selector, selector_change_sig);
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
	SE_BIND_CHILD(w, dims_sig);
	SE_BIND_CHILD(h, dims_sig);
	SE_BIND_CHILD(x, dims_sig);
	SE_BIND_CHILD(y, dims_sig);
	SE_BIND_CHILD(w_units, dims_sig);
	SE_BIND_CHILD(h_units, dims_sig);

	gtk_widget_class_bind_template_child(widget_class, StylesheetEditor, element_tree);

	gtk_widget_class_bind_template_callback(widget_class, revert_sig);

	g_signal_new("changed", COLLOQUIUM_TYPE_STYLESHEET_EDITOR,
	             G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}


StylesheetEditor *stylesheet_editor_new(Stylesheet *ss)
{
	StylesheetEditor *se;

	se = g_object_new(COLLOQUIUM_TYPE_STYLESHEET_EDITOR, NULL);
	if ( se == NULL ) return NULL;

	se->priv->stylesheet = ss;
	se->priv->style_name = "NARRATIVE";
	set_values_from_presentation(se);

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Element", renderer,
	                                                  "text", 0,
	                                                  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(se->selector), column);

	return se;
}

