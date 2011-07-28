/*
 * stylesheet.c
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
#include <libconfig.h>

#include "presentation.h"
#include "stylesheet.h"


struct _stylesheetwindow
{
	struct presentation *p;  /* Presentation to update when user alters
	                          * something in this window */
	GtkWidget           *window;
	StyleSheet          *ss; /* Style sheet this window corresponds to */

	GtkWidget           *text_font;
	GtkWidget           *text_colour;

	struct text_style *cur_text_style;
	struct layout_element *cur_layout_element;
};


struct _stylesheet
{
	/* Slide layout */
	struct layout_element **layout_elements;
	int                     n_layout_elements;

	/* Normal text styles */
	struct text_style **text_styles;
	int                 n_text_styles;

	/* Background stuff */

	/* Image styles */
};


static void text_changed_sig(GtkComboBox *combo,
                             struct _stylesheetwindow *s)
{
	int n;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	s->cur_text_style = s->ss->text_styles[n];

	gtk_font_button_set_font_name(GTK_FONT_BUTTON(s->text_font),
	                              s->cur_text_style->font);
}


static void text_font_set_sig(GtkFontButton *widget,
                              struct _stylesheetwindow *s)
{
	const gchar *font;

	font = gtk_font_button_get_font_name(widget);
	free(s->cur_text_style->font);
	s->cur_text_style->font = strdup(font);
}


static void text_colour_set_sig(GtkColorButton *widget,
                              struct _stylesheetwindow *s)
{
	GdkColor col;
	guint16 al;

	gtk_color_button_get_color(widget, &col);
	free(s->cur_text_style->colour);
	s->cur_text_style->colour = gdk_color_to_string(&col);
	al = gtk_color_button_get_alpha(widget);
	s->cur_text_style->alpha = (double)al / 65535.0;
}


static void do_text(struct _stylesheetwindow *s, GtkWidget *b)
{
	GtkWidget *table;
	GtkWidget *box;
	GtkWidget *line;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *vbox;
	int i;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(b), vbox, TRUE, TRUE, 0);

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);

	label = gtk_label_new("Style:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	combo = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), combo, 1, 4, 0, 1);

	for ( i=0; i<s->ss->n_text_styles; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          s->ss->text_styles[i]->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_widget_set_size_request(GTK_WIDGET(combo), 300, -1);
	g_signal_connect(G_OBJECT(combo), "changed",
	                 G_CALLBACK(text_changed_sig), s);

	line = gtk_hseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), line, 0, 4, 1, 2);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 10);
	gtk_table_set_row_spacing(GTK_TABLE(table), 1, 10);

	label = gtk_label_new("Font:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	s->text_font = gtk_font_button_new_with_font("Sans 12");
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 2, 3);
	gtk_box_pack_start(GTK_BOX(box), s->text_font, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(s->text_font), "font-set",
	                 G_CALLBACK(text_font_set_sig), s);

	label = gtk_label_new("Colour:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	s->text_colour = gtk_color_button_new();
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 3, 4);
	gtk_box_pack_start(GTK_BOX(box), s->text_colour, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(s->text_colour), "color-set",
	                 G_CALLBACK(text_colour_set_sig), s);

	/* Force first update */
	text_changed_sig(GTK_COMBO_BOX(combo), s);
}


static void layout_changed_sig(GtkComboBox *combo,
                               struct _stylesheetwindow *s)
{
	int n;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	s->cur_layout_element = s->ss->layout_elements[n];
}


static void do_layout(struct _stylesheetwindow *s, GtkWidget *b)
{
	GtkWidget *table;
	GtkWidget *line;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *spin;
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *entry;
	int i;

	box = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(b), box, FALSE, FALSE, 5);
	label = gtk_label_new("Element:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_new_text();
	for ( i=0; i<s->ss->n_layout_elements; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          s->ss->layout_elements[i]->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	g_signal_connect(G_OBJECT(combo), "changed",
	                 G_CALLBACK(layout_changed_sig), s);
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
	spin = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 0, 1, 1, 2);

	/* Up */
	spin = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 1, 2, 0, 1);

	/* Right */
	spin = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 2, 3, 1, 2);

	/* Down */
	spin = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 1, 2, 2, 3);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);
	label = gtk_label_new("Offset from centre:");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	table = gtk_table_new(2, 2, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	/* Up */
	label = gtk_label_new("Upwards:");
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	spin = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 1, 2, 0, 1);

	/* Right */
	label = gtk_label_new("Across:");
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	spin = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), spin, 1, 2, 1, 2);

	table = gtk_table_new(3, 2, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);
	gtk_box_pack_start(GTK_BOX(b), table, FALSE, FALSE, 5);
	label = gtk_label_new("Horizontal alignment:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	combo = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), combo, 1, 2, 0, 1);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Left");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Centre");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Right");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

	label = gtk_label_new("Vertical alignment:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	combo = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), combo, 1, 2, 1, 2);
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Top");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Centre");
	gtk_combo_box_append_text(GTK_COMBO_BOX(combo), "Bottom");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

	label = gtk_label_new("Maximum width:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	entry = gtk_entry_new_with_max_length(32);
	gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 2, 2, 3);

}



static gint destroy_stylesheet_sig(GtkWidget *w, struct _stylesheetwindow *s)
{
	s->p->stylesheetwindow = NULL;
	free(s);
	return FALSE;
}


static void add_text_style(StyleSheet *ss, struct text_style *st)
{
	int n = ss->n_text_styles;
	ss->text_styles = realloc(ss->text_styles,
	                                (n+1)*sizeof(st));

	ss->text_styles[n] = st;

	ss->n_text_styles = n+1;
}


static struct text_style *new_text_style(StyleSheet *ss, const char *name)
{
	struct text_style *st;

	st = malloc(sizeof(*st));
	if ( st == NULL ) return NULL;

	st->name = strdup(name);
	st->colour = strdup("#000000000000");  /* Black */
	st->alpha = 1.0;

	add_text_style(ss, st);

	return st;
}


static struct layout_element *new_layout_element(StyleSheet *ss,
                                                 const char *name)
{
	struct layout_element *ly;
	int n;

	ly = malloc(sizeof(*ly));
	if ( ly == NULL ) return NULL;

	ly->name = strdup(name);

	n = ss->n_layout_elements;
	ss->layout_elements = realloc(ss->layout_elements, (n+1)*sizeof(ly));
	/* Yes, the size of the pointer */

	ss->layout_elements[n] = ly;
	ss->n_layout_elements = n+1;

	return ly;
}


static void default_stylesheet(StyleSheet *ss)
{
	struct text_style *st;
	struct layout_element *ly;

	st = new_text_style(ss, "Slide title");
	st->font = strdup("Sans 40");
	ly = new_layout_element(ss, st->name);
	ly->text_style = st;
	ly->margin_left = 20.0;
	ly->margin_right = 20.0;
	ly->margin_top = 20.0;
	ly->margin_bottom = 20.0;
	ly->halign = J_CENTER;
	ly->valign = V_TOP;
	ly->offset_x = 0.0;
	ly->offset_y = 0.0;  /* irrelevant */

	st = new_text_style(ss, "Presentation title");
	st->font = strdup("Sans 50");
	ly = new_layout_element(ss, st->name);
	ly->text_style = st;
	ly->margin_left = 20.0;
	ly->margin_right = 20.0;
	ly->margin_top = 20.0;
	ly->margin_bottom = 20.0;
	ly->halign = J_CENTER;
	ly->valign = V_CENTER;
	ly->offset_x = -200.0;
	ly->offset_y = +300.0;

	st = new_text_style(ss, "Presentation author");
	st->font = strdup("Sans 30");
	ly = new_layout_element(ss, st->name);
	ly->text_style = st;
	ly->margin_left = 20.0;
	ly->margin_right = 20.0;
	ly->margin_top = 20.0;
	ly->margin_bottom = 20.0;
	ly->halign = J_CENTER;
	ly->valign = V_CENTER;
	ly->offset_x = +200.0;
	ly->offset_y = -300.0;

	st = new_text_style(ss, "Running text");
	st->font = strdup("Sans 14");

	ly = new_layout_element(ss, "Slide content");
	ly->text_style = st;
	ly->margin_left = 20.0;
	ly->margin_right = 20.0;
	ly->margin_top = 20.0;
	ly->margin_bottom = 20.0;
	ly->halign = J_CENTER;
	ly->valign = V_CENTER;
	ly->offset_x = +200.0;
	ly->offset_y = -300.0;

}


StyleSheet *new_stylesheet()
{
	StyleSheet *ss;

	ss = calloc(1, sizeof(struct _stylesheet));
	if ( ss == NULL ) return NULL;

	ss->n_text_styles = 0;
	ss->text_styles = NULL;
	ss->n_layout_elements = 0;
	ss->layout_elements = NULL;
	default_stylesheet(ss);

	return ss;
}


void save_stylesheet(StyleSheet *ss, const char *filename)
{
	config_t config;

	config_init(&config);

	if ( config_write_file(&config, filename) == CONFIG_FALSE ) {
		printf("Couldn't save style sheet to '%s'\n", filename);
	}
	config_destroy(&config);
}


StyleSheet *load_stylesheet(const char *filename)
{
	StyleSheet *ss;
	config_t config;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

	config_init(&config);
	if ( config_read_file(&config, filename) != CONFIG_TRUE ) {
		printf("Style sheet parse error: %s at line %i.\n",
		        config_error_text(&config),
		        config_error_line(&config));
		config_destroy(&config);
		default_stylesheet(ss);
		return ss;
	}



	config_destroy(&config);

	return ss;
}


StylesheetWindow *open_stylesheet(struct presentation *p)
{
	struct _stylesheetwindow *s;
	GtkWidget *nb;
	GtkWidget *text_box;
	GtkWidget *background_box;

	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;

	s->p = p;
	s->ss = p->ss;
	s->cur_text_style = NULL;
	s->cur_layout_element = NULL;

	s->window = gtk_dialog_new_with_buttons("Stylesheet",
	                                   GTK_WINDOW(p->window), 0,
	                                   GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
	                                   NULL);
	gtk_dialog_set_has_separator(GTK_DIALOG(s->window), FALSE);

	nb = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(nb), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(s->window)->vbox), nb,
	                   TRUE, TRUE, 0);

	text_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(text_box), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), text_box,
	                         gtk_label_new("Text styles"));
	do_text(s, text_box);

	text_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(text_box), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), text_box,
	                         gtk_label_new("Slide layout"));
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
