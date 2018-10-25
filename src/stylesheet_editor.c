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


static int colour_duo_parse(const char *a, GdkRGBA *col1, GdkRGBA *col2)
{
	char *acopy;
	char *n2;

	acopy = strdup(a);
	if ( acopy == NULL ) return 1;

	n2 = strchr(acopy, ',');
	if ( n2 == NULL ) {
		fprintf(stderr, _("Invalid bg gradient spec '%s'\n"), a);
		return 1;
	}

	n2[0] = '\0';

	if ( gdk_rgba_parse(col1, acopy) != TRUE ) {
		fprintf(stderr, _("Failed to parse colour: %s\n"), acopy);
	}
	if ( gdk_rgba_parse(col2, &n2[1]) != TRUE ) {
		fprintf(stderr, _("Failed to parse colour: %s\n"), &n2[1]);
	}

	free(acopy);
	return 0;
}


static void set_font_from_ss(Stylesheet *ss, const char *path, GtkWidget *w)
{
	char *result = stylesheet_lookup(ss, path);
	if ( result != NULL ) {
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(w), result);
	}
}


static void set_col_from_ss(Stylesheet *ss, const char *path, GtkWidget *w)
{
	char *result = stylesheet_lookup(ss, path);
	if ( result != NULL ) {
		GdkRGBA rgba;
		if ( gdk_rgba_parse(&rgba, result) == TRUE ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &rgba);
		}
	}
}


static void set_vals_from_ss(Stylesheet *ss, const char *path,
                             GtkWidget *wl, GtkWidget *wr,
                             GtkWidget *wt, GtkWidget *wb)
{
	char *result = stylesheet_lookup(ss, path);
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
	char *result = stylesheet_lookup(ss, path);
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

static void set_bg_from_ss(Stylesheet *ss, const char *path, GtkWidget *wcol,
                           GtkWidget *wcol2, GtkWidget *wgrad)
{
	char *result;
	char fullpath[256];
	int found = 0;

	strcpy(fullpath, path);
	strcat(fullpath, ".bgcol");
	result = stylesheet_lookup(ss, fullpath);
	if ( result != NULL ) {
		GdkRGBA rgba;
		found = 1;
		if ( gdk_rgba_parse(&rgba, result) == TRUE ) {
			if ( rgba.alpha == 0.0 ) {
				gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "none");
				gtk_widget_set_sensitive(wcol, FALSE);
				gtk_widget_set_sensitive(wcol2, FALSE);
			} else {
				gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba);
				gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "flat");
				gtk_widget_set_sensitive(wcol, TRUE);
				gtk_widget_set_sensitive(wcol2, FALSE);
			}
		} else {
			fprintf(stderr, _("Failed to parse colour: %s\n"), result);
		}
	}

	strcpy(fullpath, path);
	strcat(fullpath, ".bggradv");
	result = stylesheet_lookup(ss, fullpath);
	if ( result != NULL ) {
		GdkRGBA rgba1, rgba2;
		found = 1;
		if ( colour_duo_parse(result, &rgba1, &rgba2) == 0 ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba1);
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol2), &rgba2);
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "vert");
				gtk_widget_set_sensitive(wcol, TRUE);
				gtk_widget_set_sensitive(wcol2, TRUE);
		}
	}

	strcpy(fullpath, path);
	strcat(fullpath, ".bggradh");
	result = stylesheet_lookup(ss, fullpath);
	if ( result != NULL ) {
		GdkRGBA rgba1, rgba2;
		found = 1;
		if ( colour_duo_parse(result, &rgba1, &rgba2) == 0 ) {
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol), &rgba1);
			gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(wcol2), &rgba2);
			gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "horiz");
			gtk_widget_set_sensitive(wcol, TRUE);
			gtk_widget_set_sensitive(wcol2, TRUE);
		}
	}

	if ( !found ) {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(wgrad), "none");
		gtk_widget_set_sensitive(wcol, FALSE);
		gtk_widget_set_sensitive(wcol2, FALSE);
	}
}


static void set_values_from_presentation(StylesheetEditor *se)
{
	Stylesheet *ss = se->priv->p->stylesheet;

	/* Narrative */
	set_font_from_ss(ss, "$.narrative.font", se->narrative_style_font);
	set_col_from_ss(ss, "$.narrative.fgcol", se->narrative_style_fgcol);
	set_bg_from_ss(ss, "$.narrative", se->narrative_style_bgcol,
	                                  se->narrative_style_bgcol2,
	                                  se->narrative_style_bggrad);
	set_vals_from_ss(ss, "$.narrative.pad", se->narrative_style_padding_l,
	                                        se->narrative_style_padding_r,
	                                        se->narrative_style_padding_t,
	                                        se->narrative_style_padding_b);
	set_vals_from_ss(ss, "$.narrative.paraspace", se->narrative_style_paraspace_l,
	                                              se->narrative_style_paraspace_r,
	                                              se->narrative_style_paraspace_t,
	                                              se->narrative_style_paraspace_b);

	/* Slides */
	set_size_from_ss(ss, "$.slide.size", se->slide_size_w, se->slide_size_h);
	set_bg_from_ss(ss, "$.slide", se->slide_style_bgcol,
	                              se->slide_style_bgcol2,
	                              se->slide_style_bggrad);


	/* Frames */
	set_font_from_ss(ss, "$.slide.frame.font", se->frame_style_font);
	set_col_from_ss(ss, "$.slide.frame.fgcol", se->frame_style_fgcol);
	set_bg_from_ss(ss, "$.slide.frame", se->frame_style_bgcol,
	                                    se->frame_style_bgcol2,
	                                    se->frame_style_bggrad);
	set_vals_from_ss(ss, "$.slide.frame.pad", se->frame_style_padding_l,
	                                          se->frame_style_padding_r,
	                                          se->frame_style_padding_t,
	                                          se->frame_style_padding_b);
	set_vals_from_ss(ss, "$.slide.frame.paraspace", se->frame_style_paraspace_l,
	                                                se->frame_style_paraspace_r,
	                                                se->frame_style_paraspace_t,
	                                                se->frame_style_paraspace_b);
}


static void update_bg(struct presentation *p, const char *style_name,
                      GradientType bggrad, GdkRGBA col1, GdkRGBA col2)
{
	/* FIXME: set in JSON */
}


static void revert_sig(GtkButton *button, StylesheetEditor *widget)
{
	printf("click revert!\n");
}


static GradientType id_to_gradtype(const gchar *id)
{
	assert(id != NULL);
	if ( strcmp(id, "flat") == 0 ) return GRAD_NONE;
	if ( strcmp(id, "horiz") == 0 ) return GRAD_HORIZ;
	if ( strcmp(id, "vert") == 0 ) return GRAD_VERT;
	if ( strcmp(id, "none") == 0 ) return GRAD_NOBG;
	return GRAD_NONE;
}


static void set_font(GtkFontButton *widget, StylesheetEditor *se,
                     const char *style_name)
{
//	const gchar *font;
//	font = gtk_font_button_get_font_name(GTK_FONT_BUTTON(widget));
	/* FIXME: set in JSON */
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
	/* FIXME: Set in JSON */
	g_free(col);
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
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),
	                           &se->narrative_bgcol);
	update_bg(se->priv->p, "narrative", se->narrative_bggrad,
	          se->narrative_bgcol, se->narrative_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_bgcol2_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),
	                           &se->narrative_bgcol2);
	update_bg(se->priv->p, "narrative", se->narrative_bggrad,
	          se->narrative_bgcol, se->narrative_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void narrative_bggrad_sig(GtkComboBox *widget, StylesheetEditor *se)
{
	const gchar *id;
	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	se->narrative_bggrad = id_to_gradtype(id);
	update_bg(se->priv->p, "narrative", se->narrative_bggrad,
	          se->narrative_bgcol, se->narrative_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void slide_size_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
}


static void slide_bgcol_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),
	                           &se->slide_bgcol);
	update_bg(se->priv->p, "slide", se->slide_bggrad,
	          se->slide_bgcol, se->slide_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void slide_bgcol2_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),
	                           &se->slide_bgcol2);
	update_bg(se->priv->p, "slide", se->slide_bggrad,
	          se->slide_bgcol, se->slide_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void slide_bggrad_sig(GtkComboBox *widget, StylesheetEditor *se)
{
	const gchar *id;

	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	se->slide_bggrad = id_to_gradtype(id);
	update_bg(se->priv->p, "slide", se->slide_bggrad,
	          se->slide_bgcol, se->slide_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
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
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),
	                           &se->frame_bgcol);
	update_bg(se->priv->p, "frame", se->frame_bggrad,
	          se->frame_bgcol, se->frame_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_bgcol2_sig(GtkColorButton *widget, StylesheetEditor *se)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget),
	                           &se->frame_bgcol);
	update_bg(se->priv->p, "frame", se->frame_bggrad,
	          se->frame_bgcol, se->frame_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_bggrad_sig(GtkComboBox *widget, StylesheetEditor *se)
{
	const gchar *id;
	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
	se->frame_bggrad = id_to_gradtype(id);
	update_bg(se->priv->p, "frame", se->frame_bggrad,
	          se->frame_bgcol, se->frame_bgcol2);

	set_values_from_presentation(se);
	g_signal_emit_by_name(se, "changed");
}


static void frame_padding_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
}


static void frame_paraspace_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
}


static void narrative_padding_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
}


static void narrative_paraspace_sig(GtkSpinButton *widget, StylesheetEditor *se)
{
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
	SE_BIND_CHILD(narrative_style_paraspace_l, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_paraspace_r, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_paraspace_t, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_paraspace_b, narrative_paraspace_sig);
	SE_BIND_CHILD(narrative_style_padding_l, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_padding_r, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_padding_t, narrative_padding_sig);
	SE_BIND_CHILD(narrative_style_padding_b, narrative_padding_sig);

	/* Slide style */
	SE_BIND_CHILD(slide_size_w, slide_size_sig);
	SE_BIND_CHILD(slide_size_h, slide_size_sig);
	SE_BIND_CHILD(slide_style_bgcol, slide_bgcol_sig);
	SE_BIND_CHILD(slide_style_bgcol2, slide_bgcol2_sig);
	SE_BIND_CHILD(slide_style_bggrad, slide_bggrad_sig);

	/* Slide->frame style */
	SE_BIND_CHILD(frame_style_font, frame_font_sig);
	SE_BIND_CHILD(frame_style_fgcol, frame_fgcol_sig);
	SE_BIND_CHILD(frame_style_bgcol, frame_bgcol_sig);
	SE_BIND_CHILD(frame_style_bgcol2, frame_bgcol2_sig);
	SE_BIND_CHILD(frame_style_bggrad, frame_bggrad_sig);
	SE_BIND_CHILD(frame_style_paraspace_l, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_paraspace_r, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_paraspace_t, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_paraspace_b, frame_paraspace_sig);
	SE_BIND_CHILD(frame_style_padding_l, frame_padding_sig);
	SE_BIND_CHILD(frame_style_padding_r, frame_padding_sig);
	SE_BIND_CHILD(frame_style_padding_t, frame_padding_sig);
	SE_BIND_CHILD(frame_style_padding_b, frame_padding_sig);

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

