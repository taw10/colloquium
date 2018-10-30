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
#include "stylesheet.h"
#include "utils.h"


G_DEFINE_TYPE_WITH_CODE(StylesheetEditor, stylesheet_editor,
                        GTK_TYPE_DIALOG, NULL)


struct _sspriv
{
	struct presentation *p;
};


static void set_font_from_ss(Stylesheet *ss, const char *path, GtkWidget *w)
{
	char *result = stylesheet_lookup(ss, path, "font");
	if ( result != NULL ) {
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(w), result);
	}
}


static void set_col_from_ss(Stylesheet *ss, const char *path, GtkWidget *w)
{
	char *result = stylesheet_lookup(ss, path, "fgcol");
	if ( result != NULL ) {
		GdkRGBA rgba;
		if ( gdk_rgba_parse(&rgba, result) == TRUE ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &rgba);
		}
	}
}


static void set_vals_from_ss(Stylesheet *ss, const char *path, const char *key,
                             GtkWidget *wl, GtkWidget *wr,
                             GtkWidget *wt, GtkWidget *wb)
{
	char *result = stylesheet_lookup(ss, path, key);
	if ( result != NULL ) {
		float v[4];
		if ( parse_tuple(result, v) == 0 ) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(wl), v[0]);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(wr), v[1]);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(wt), v[2]);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(wb), v[3]);
		} else {
			fprintf(stderr, _("Failed to parse quad: %s\n"), result);
		}
	} else {
		printf("Not found %s\n", path);
	}
}


static void set_size_from_ss(Stylesheet *ss, const char *path,
                             GtkWidget *ww, GtkWidget *wh)
{
	char *result = stylesheet_lookup(ss, path, "size");
	if ( result != NULL ) {
		float v[2];
		if ( parse_double(result, v) == 0 ) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(ww), v[0]);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(wh), v[1]);
		} else {
			fprintf(stderr, _("Failed to parse double: %s\n"), result);
		}
	} else {
		printf("Not found %s\n", path);
	}
}


static int alignment_ok(const char *a)
{
	if ( a == NULL ) return 0;
	if ( strcmp(a, "left") == 0 ) return 1;
	if ( strcmp(a, "center") == 0 ) return 1;
	if ( strcmp(a, "right") == 0 ) return 1;
	return 0;
}


static void set_alignment_from_ss(Stylesheet *ss, const char *path,
                                  GtkWidget *d)
{
	char *result = stylesheet_lookup(ss, path, "alignment");
	if ( alignment_ok(result) ) {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(d), result);
	}
}


static void set_bg_from_ss(Stylesheet *ss, const char *path, GtkWidget *wcol,
                           GtkWidget *wcol2, GtkWidget *wgrad)
{
	char *result;
	int found = 0;

	result = stylesheet_lookup(ss, path, "bgcol");
	if ( result != NULL ) {
		GdkRGBA rgba;
		found = 1;
		if ( gdk_rgba_parse(&rgba, result) == TRUE ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba);
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "flat");
			gtk_widget_set_sensitive(wcol, TRUE);
			gtk_widget_set_sensitive(wcol2, FALSE);
		} else {
			fprintf(stderr, _("Failed to parse colour: %s\n"), result);
		}
	}

	result = stylesheet_lookup(ss, path, "bggradv");
	if ( result != NULL ) {
		GdkRGBA rgba1, rgba2;
		found = 1;
		if ( parse_colour_duo(result, &rgba1, &rgba2) == 0 ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba1);
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol2), &rgba2);
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "vert");
				gtk_widget_set_sensitive(wcol, TRUE);
				gtk_widget_set_sensitive(wcol2, TRUE);
		}
	}

	result = stylesheet_lookup(ss, path, "bggradh");
	if ( result != NULL ) {
		GdkRGBA rgba1, rgba2;
		found = 1;
		if ( parse_colour_duo(result, &rgba1, &rgba2) == 0 ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba1);
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol2), &rgba2);
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "horiz");
			gtk_widget_set_sensitive(wcol, TRUE);
			gtk_widget_set_sensitive(wcol2, TRUE);
		}
	}

	if ( !found ) {
		GdkRGBA rgba;
		rgba.red = 1.0;
		rgba.green = 1.0;
		rgba.blue = 1.0;
		rgba.alpha = 0.0;
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "flat");
		gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba);
		gtk_widget_set_sensitive(wcol, TRUE);
		gtk_widget_set_sensitive(wcol2, FALSE);
	}
}


static void set_values_from_presentation(StylesheetEditor *se)
{
	Stylesheet *ss = se->priv->p->stylesheet;

	/* Narrative */
	set_font_from_ss(ss, "$.narrative", se->narrative_style_font);
	set_col_from_ss(ss, "$.narrative", se->narrative_style_fgcol);
	set_alignment_from_ss(ss, "$.narrative", se->narrative_style_alignment);
	set_bg_from_ss(ss, "$.narrative", se->narrative_style_bgcol,
	                                  se->narrative_style_bgcol2,
	                                  se->narrative_style_bggrad);
	set_vals_from_ss(ss, "$.narrative", "pad", se->narrative_style_padding_l,
	                                           se->narrative_style_padding_r,
	                                           se->narrative_style_padding_t,
	                                           se->narrative_style_padding_b);
	set_vals_from_ss(ss, "$.narrative", "paraspace", se->narrative_style_paraspace_l,
	                                                 se->narrative_style_paraspace_r,
	                                                 se->narrative_style_paraspace_t,
	                                                 se->narrative_style_paraspace_b);

	/* Slides */
	set_size_from_ss(ss, "$.slide", se->slide_size_w, se->slide_size_h);
	set_bg_from_ss(ss, "$.slide", se->slide_style_bgcol,
	                              se->slide_style_bgcol2,
	                              se->slide_style_bggrad);


	/* Frames */
	set_font_from_ss(ss, "$.slide.frame", se->frame_style_font);
	set_col_from_ss(ss, "$.slide.frame", se->frame_style_fgcol);
	set_alignment_from_ss(ss, "$.slide.frame", se->frame_style_alignment);
	set_bg_from_ss(ss, "$.slide.frame", se->frame_style_bgcol,
	                                    se->frame_style_bgcol2,
	                                    se->frame_style_bggrad);
	set_vals_from_ss(ss, "$.slide.frame", "pad", se->frame_style_padding_l,
	                                             se->frame_style_padding_r,
	                                             se->frame_style_padding_t,
	                                             se->frame_style_padding_b);
	set_vals_from_ss(ss, "$.slide.frame", "paraspace", se->frame_style_paraspace_l,
	                                                   se->frame_style_paraspace_r,
	                                                   se->frame_style_paraspace_t,
	                                                   se->frame_style_paraspace_b);
}


static GradientType id_to_gradtype(const gchar *id)
{
	assert(id != NULL);
	if ( strcmp(id, "flat") == 0 ) return GRAD_NONE;
	if ( strcmp(id, "horiz") == 0 ) return GRAD_HORIZ;
	if ( strcmp(id, "vert") == 0 ) return GRAD_VERT;
	return GRAD_NONE;
}


static void update_bg(struct presentation *p, const char *style_name,
                      GtkWidget *bggradw, GtkWidget *col1w, GtkWidget*col2w)
{
	GradientType g;
	const gchar *id;
	GdkRGBA rgba;
	gchar *col1;
	gchar *col2;
	gchar *gradient;

	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(bggradw));
	g = id_to_gradtype(id);

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(col1w), &rgba);
	if ( rgba.alpha < 0.000001 ) rgba.alpha = 0.0;
	col1 = gdk_rgba_to_string(&rgba);

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(col2w), &rgba);
	col2 = gdk_rgba_to_string(&rgba);

	gradient = g_strconcat(col1, ",", col2, NULL);

	switch ( g ) {

		case GRAD_NONE :
		stylesheet_set(p->stylesheet, style_name, "bgcol",
		               col1);
		stylesheet_delete(p->stylesheet, style_name, "bggradv");
		stylesheet_delete(p->stylesheet, style_name, "bggradh");
		break;

		case GRAD_HORIZ :
		stylesheet_set(p->stylesheet, style_name, "bggradh",
		               gradient);
		stylesheet_delete(p->stylesheet, style_name, "bggradv");
		stylesheet_delete(p->stylesheet, style_name, "bgcol");
		break;

		case GRAD_VERT :
		stylesheet_set(p->stylesheet, style_name, "bggradv",
		               gradient);
		stylesheet_delete(p->stylesheet, style_name, "bggradh");
		stylesheet_delete(p->stylesheet, style_name, "bgcol");
		break;

	}

	g_free(gradient);
	g_free(col1);
	g_free(col2);
}


static void update_spacing(struct presentation *p, const char *style_name,
                           const char *key, GtkWidget *wl, GtkWidget *wr,
                           GtkWidget *wt, GtkWidget *wb)
{
	int v[4];
	char tmp[256];

	v[0] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(wl));
	v[1] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(wr));
	v[2] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(wt));
	v[3] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(wb));

	if ( snprintf(tmp, 256, "%i,%i,%i,%i", v[0], v[1], v[2], v[3]) >= 256 ) {
		fprintf(stderr, _("Spacing too long\n"));
	} else {
		stylesheet_set(p->stylesheet, style_name, key, tmp);
	}
}


static void revert_sig(GtkButton *button, StylesheetEditor *widget)
{
	printf("click revert!\n");
}


static void set_font(GtkFontButton *widget, StylesheetEditor *se,
                     const char *style_name)
{
	const gchar *font;
	font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));

	stylesheet_set(se->priv->p->stylesheet, style_name, "font", font);
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
	stylesheet_set(se->priv->p->stylesheet, style_name, "fgcol", col);
	g_free(col);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_font_sig(GtkFontButton *widget, StylesheetEditor *se)
{
	set_font(widget, se, "$.narrative");
}


static void narrative_fgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "$.narrative", "fgcol");
}


static void narrative_bg_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	update_bg(se->priv->p, "$.narrative",
	          se->narrative_style_bggrad,
	          se->narrative_style_bgcol,
	          se->narrative_style_bgcol2);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_alignment_sig(GtkComboBoxText *widget, StylesheetEditor *se)
{
	const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	stylesheet_set(se->priv->p->stylesheet, "$.narrative", "alignment", id);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void slide_size_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	int w, h;
	char tmp[256];

	w = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(se->slide_size_w));
	h = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(se->slide_size_h));

	if ( snprintf(tmp, 256, "%ix%i", w, h) >= 256 ) {
		fprintf(stderr, _("Slide size too long\n"));
	} else {
		stylesheet_set(se->priv->p->stylesheet, "$.slide", "size", tmp);
		se->priv->p->slide_width = w;
		se->priv->p->slide_height = h;
	}

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void slide_bg_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	update_bg(se->priv->p, "$.slide",
	          se->slide_style_bggrad,
	          se->slide_style_bgcol,
	          se->slide_style_bgcol2);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_font_sig(GtkFontButton *widget, StylesheetEditor *se)
{
	set_font(widget, se, "$.slide.frame");
}


static void frame_fgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	set_col(widget, se, "$.slide.frame", "fgcol");
}


static void frame_bg_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	update_bg(se->priv->p, "$.slide.frame",
	          se->frame_style_bggrad,
	          se->frame_style_bgcol,
	          se->frame_style_bgcol2);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_padding_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	update_spacing(se->priv->p, "$.slide.frame", "pad",
	               se->frame_style_padding_l,
	               se->frame_style_padding_r,
	               se->frame_style_padding_t,
	               se->frame_style_padding_b);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_paraspace_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	update_spacing(se->priv->p, "$.slide.frame", "paraspace",
	               se->frame_style_paraspace_l,
	               se->frame_style_paraspace_r,
	               se->frame_style_paraspace_t,
	               se->frame_style_paraspace_b);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_alignment_sig(GtkComboBoxText *widget, StylesheetEditor *se)
{
	const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	stylesheet_set(se->priv->p->stylesheet, "$.slide.frame", "alignment", id);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_padding_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	update_spacing(se->priv->p, "$.narrative", "pad",
	               se->narrative_style_padding_l,
	               se->narrative_style_padding_r,
	               se->narrative_style_padding_t,
	               se->narrative_style_padding_b);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_paraspace_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
	update_spacing(se->priv->p, "$.narrative", "paraspace",
	               se->narrative_style_paraspace_l,
	               se->narrative_style_paraspace_r,
	               se->narrative_style_paraspace_t,
	               se->narrative_style_paraspace_b);
	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
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
	SE_BIND_CHILD(narrative_style_bgcol, narrative_bg_sig);
	SE_BIND_CHILD(narrative_style_bgcol2, narrative_bg_sig);
	SE_BIND_CHILD(narrative_style_bggrad, narrative_bg_sig);
	SE_BIND_CHILD(narrative_style_paraspace_l, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_paraspace_r, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_paraspace_t, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_paraspace_b, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_padding_l, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_padding_r, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_padding_t, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_padding_b, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_alignment, narrative_alignment_sig);

	/* Slide style */
	SE_BIND_CHILD(slide_size_w, slide_size_sig);
	SE_BIND_CHILD(slide_size_h, slide_size_sig);
	SE_BIND_CHILD(slide_style_bgcol, slide_bg_sig);
	SE_BIND_CHILD(slide_style_bgcol2, slide_bg_sig);
	SE_BIND_CHILD(slide_style_bggrad, slide_bg_sig);

	/* Slide->frame style */
	SE_BIND_CHILD(frame_style_font, frame_font_sig);
	SE_BIND_CHILD(frame_style_fgcol, frame_fgcol_sig);
	SE_BIND_CHILD(frame_style_bgcol, frame_bg_sig);
	SE_BIND_CHILD(frame_style_bgcol2, frame_bg_sig);
	SE_BIND_CHILD(frame_style_bggrad, frame_bg_sig);
	SE_BIND_CHILD(frame_style_paraspace_l, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_paraspace_r, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_paraspace_t, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_paraspace_b, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_padding_l, frame_padding_sig);
	SE_BIND_CHILD(frame_style_padding_r, frame_padding_sig);
	SE_BIND_CHILD(frame_style_padding_t, frame_padding_sig);
	SE_BIND_CHILD(frame_style_padding_b, frame_padding_sig);
	SE_BIND_CHILD(frame_style_alignment, frame_alignment_sig);

	gtk_widget_class_bind_template_callback(widget_class, revert_sig);

	g_signal_new("changed", COLLOQUIUM_TYPE_STYLESHEET_EDITOR,
	             G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
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

