/*
 * stylesheet-editor.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <assert.h>

#include "presentation.h"
#include "stylesheet.h"
#include "objects.h"
#include "loadsave.h"
#include "storycode.h"


struct _stylesheetwindow
{
	struct presentation   *p;  /* Presentation to update when user alters
	                            * something in this window */
	GtkWidget             *window;
	StyleSheet            *ss; /* Style sheet this window corresponds to */

	GtkWidget             *margin_left;
	GtkWidget             *margin_right;
	GtkWidget             *margin_top;
	GtkWidget             *margin_bottom;

	GtkWidget             *text_font;
	GtkWidget             *text_colour;

	char                  *font;
	char                  *colour;
	double                 alpha;

	struct slide_template *cur_slide_template;
	struct frame_class    *cur_frame_class;
};


static void text_font_set_sig(GtkFontButton *widget,
                              struct _stylesheetwindow *s)
{
	const gchar *font;

	font = gtk_font_button_get_font_name(widget);
	free(s->font);
	s->font = strdup(font);

//	notify_style_update(s->p, s->cur_frame_class);
}


static void text_colour_set_sig(GtkColorButton *widget,
                              struct _stylesheetwindow *s)
{
	GdkColor col;
	guint16 al;

	gtk_color_button_get_color(widget, &col);
	free(s->colour);
	s->colour = gdk_color_to_string(&col);
	al = gtk_color_button_get_alpha(widget);
	s->alpha = (double)al / 65535.0;

//	notify_style_update(s->p, s->cur_frame_class);
}


static void margin_left_changed_sig(GtkSpinButton *spin,
                                    struct _stylesheetwindow *s)
{
	s->cur_frame_class->margin_left = gtk_spin_button_get_value(spin);
//	notify_style_update(s->p, s->cur_frame_class);
}


static void margin_right_changed_sig(GtkSpinButton *spin,
                                     struct _stylesheetwindow *s)
{
	s->cur_frame_class->margin_right = gtk_spin_button_get_value(spin);
//	notify_style_update(s->p, s->cur_frame_class);
}


static void margin_top_changed_sig(GtkSpinButton *spin,
                                   struct _stylesheetwindow *s)
{
	s->cur_frame_class->margin_top = gtk_spin_button_get_value(spin);
//	notify_style_update(s->p, s->cur_frame_class);
}


static void margin_bottom_changed_sig(GtkSpinButton *spin,
                                      struct _stylesheetwindow *s)
{
	s->cur_frame_class->margin_bottom = gtk_spin_button_get_value(spin);
//	notify_style_update(s->p, s->cur_frame_class);
}


static void frame_class_changed_sig(GtkComboBox *combo,
                                    struct _stylesheetwindow *s)
{
	int n;
	GdkColor col;
	char *font;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	s->cur_frame_class = s->cur_slide_template->frame_classes[n];

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_left),
	                          s->cur_frame_class->margin_left);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_right),
	                          s->cur_frame_class->margin_right);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_bottom),
	                          s->cur_frame_class->margin_bottom);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_top),
	                          s->cur_frame_class->margin_top);

	font = sc_get_final_font(s->cur_frame_class->sc_prologue);
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(s->text_font), font);

	s->colour = sc_get_final_text_colour(s->cur_frame_class->sc_prologue);
	gdk_color_parse(s->colour, &col);
	gtk_color_button_set_color(GTK_COLOR_BUTTON(s->text_colour), &col);
}


static void slide_template_changed_sig(GtkComboBox *combo,
                                       struct _stylesheetwindow *s)
{
	for ( i=0; i<s->ss->n_styles; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          s->ss->styles[i]->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
}


static void do_layout(struct _stylesheetwindow *s, GtkWidget *b)
{
	GtkWidget *table;
	GtkWidget *line;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *box;
	GtkWidget *vbox;
	int i;

	box = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(b), box, FALSE, FALSE, 5);
	label = gtk_label_new("Top-level frame:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_new_text();
	g_signal_connect(G_OBJECT(combo), "changed",
	                 G_CALLBACK(style_changed_sig), s);
	gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);

	line = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(b), line, FALSE, FALSE, 5);

	box = gtk_hbox_new(TRUE, 30);
	gtk_box_pack_start(GTK_BOX(b), box, FALSE, FALSE, 5);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);
	label = gtk_label_new("Margins:");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	table = gtk_table_new(3, 3, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	/* Left */
	s->margin_left = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), s->margin_left,
	                          0, 1, 1, 2);
	g_signal_connect(G_OBJECT(s->margin_left), "value-changed",
	                 G_CALLBACK(margin_left_changed_sig), s);

	/* Up */
	s->margin_top = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), s->margin_top,
	                          1, 2, 0, 1);
	g_signal_connect(G_OBJECT(s->margin_top), "value-changed",
	                 G_CALLBACK(margin_top_changed_sig), s);

	/* Right */
	s->margin_right = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), s->margin_right,
	                          2, 3, 1, 2);
	g_signal_connect(G_OBJECT(s->margin_right), "value-changed",
	                 G_CALLBACK(margin_right_changed_sig), s);

	/* Down */
	s->margin_bottom = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), s->margin_bottom,
	                          1, 2, 2, 3);
	g_signal_connect(G_OBJECT(s->margin_bottom), "value-changed",
	                 G_CALLBACK(margin_bottom_changed_sig), s);

	/* Font/colour stuff */
	label = gtk_label_new("Font:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	s->text_font = gtk_font_button_new_with_font("Sans 12");
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 3, 4);
	gtk_box_pack_start(GTK_BOX(box), s->text_font, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(s->text_font), "font-set",
	                 G_CALLBACK(text_font_set_sig), s);

	label = gtk_label_new("Colour:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 4, 5);
	s->text_colour = gtk_color_button_new();
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 4, 5);
	gtk_box_pack_start(GTK_BOX(box), s->text_colour, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(s->text_colour), "color-set",
	                 G_CALLBACK(text_colour_set_sig), s);

	/* Force first update */
	frame_class_changed_sig(GTK_COMBO_BOX(combo), s);
}



static gint destroy_stylesheet_sig(GtkWidget *w, struct _stylesheetwindow *s)
{
	s->p->stylesheetwindow = NULL;
	free(s);
	return FALSE;
}


StylesheetWindow *open_stylesheet(struct presentation *p)
{
	struct _stylesheetwindow *s;
	GtkWidget *nb;
	GtkWidget *text_box;
	GtkWidget *background_box;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *combo;

	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;

	s->p = p;
	s->ss = p->ss;
	s->cur_slide_template = NULL;
	s->cur_frame_class = NULL;

	s->window = gtk_dialog_new_with_buttons("Stylesheet",
	                                   GTK_WINDOW(p->window), 0,
	                                   GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
	                                   NULL);
	gtk_dialog_set_has_separator(GTK_DIALOG(s->window), TRUE);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(s->window)->vbox), box,
	                   TRUE, TRUE, 0);

	label = gtk_label_new("Slide template:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_new_text();
//	for ( i=0; i<s->ss->n_styles; i++ ) {
//		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
//		                          s->ss->styles[i]->name);
//	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
//	g_signal_connect(G_OBJECT(combo), "changed",
//	                 G_CALLBACK(template_changed_sig), s);
	gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);

	nb = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(s->window)->vbox), nb,
	                   TRUE, TRUE, 0);

	text_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(text_box), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), text_box,
	                         gtk_label_new("Top Level Frames"));
	do_layout(s, text_box);

	background_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(background_box), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), background_box,
	                         gtk_label_new("Background"));

	g_signal_connect(G_OBJECT(s->window), "destroy",
	                 G_CALLBACK(destroy_stylesheet_sig), s);
	g_signal_connect(G_OBJECT(s->window), "response",
	                 G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show_all(s->window);

	return s;
}
