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

	GtkWidget           *fixed_font;

	struct fixed_text_style *cur_fixed_style;
};


struct fixed_text_style
{
	char *name;
	char *font;
};


struct text_style
{
	char *name;
	char *font;
};


struct _stylesheet
{
	/* Fixed text styles */
	struct fixed_text_style **fixed_text_styles;
	int                 n_fixed_text_styles;

	/* Normal text styles */
	struct text_style **text_styles;
	int                 n_text_styles;

	/* Background stuff */

	/* Image styles */
};

static void do_text(struct _stylesheetwindow *s, GtkWidget *b)
{
	GtkWidget *table;
	GtkWidget *box;
	GtkWidget *line;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *font;
	GtkWidget *colour;
	int i;

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(b), table, TRUE, TRUE, 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);

	label = gtk_label_new("Style:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	combo = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), combo, 1, 2, 0, 1);

	for ( i=0; i<s->ss->n_text_styles; i++ ) {

		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          s->ss->text_styles[i]->name);

	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_widget_set_size_request(GTK_WIDGET(combo), 300, -1);

	line = gtk_hseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), line, 0, 4, 1, 2);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 10);
	gtk_table_set_row_spacing(GTK_TABLE(table), 1, 10);

	label = gtk_label_new("Font:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	font = gtk_font_button_new_with_font("Sans 12");
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 2, 3);
	gtk_box_pack_start(GTK_BOX(box), font, FALSE, FALSE, 0);

	label = gtk_label_new("Colour:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	colour = gtk_color_button_new();
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 3, 4);
	gtk_box_pack_start(GTK_BOX(box), colour, FALSE, FALSE, 0);
}


static void fixed_text_changed_sig(GtkComboBox *combo,
                                   struct _stylesheetwindow *s)
{
	int n;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	s->cur_fixed_style = s->ss->fixed_text_styles[n];

	gtk_font_button_set_font_name(GTK_FONT_BUTTON(s->fixed_font),
	                              s->cur_fixed_style->font);
}


static void fixed_font_set_sig(GtkFontButton *widget,
                               struct _stylesheetwindow *s)
{
	const gchar *font;

	font = gtk_font_button_get_font_name(widget);
	free(s->cur_fixed_style->font);
	s->cur_fixed_style->font = strdup(font);
}


static void do_fixed_text(struct _stylesheetwindow *s, GtkWidget *b)
{
	GtkWidget *table;
	GtkWidget *box;
	GtkWidget *line;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *colour;
	int i;

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(b), table, TRUE, TRUE, 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);

	label = gtk_label_new("Style:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	combo = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), combo, 1, 2, 0, 1);

	for ( i=0; i<s->ss->n_fixed_text_styles; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          s->ss->fixed_text_styles[i]->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_widget_set_size_request(GTK_WIDGET(combo), 300, -1);
	g_signal_connect(G_OBJECT(combo), "changed",
	                 G_CALLBACK(fixed_text_changed_sig), s);

	line = gtk_hseparator_new();
	gtk_table_attach_defaults(GTK_TABLE(table), line, 0, 4, 1, 2);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 10);
	gtk_table_set_row_spacing(GTK_TABLE(table), 1, 10);

	label = gtk_label_new("Font:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	s->fixed_font = gtk_font_button_new_with_font("Sans 12");
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 2, 3);
	gtk_box_pack_start(GTK_BOX(box), s->fixed_font, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(s->fixed_font), "font-set",
	                 G_CALLBACK(fixed_font_set_sig), s);

	label = gtk_label_new("Colour:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	colour = gtk_color_button_new();
	box = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 3, 4);
	gtk_box_pack_start(GTK_BOX(box), colour, FALSE, FALSE, 0);

	/* Force first update */
	fixed_text_changed_sig(GTK_COMBO_BOX(combo), s);
}


static gint destroy_stylesheet_sig(GtkWidget *w, struct _stylesheetwindow *s)
{
	s->p->stylesheetwindow = NULL;
	free(s);
	return FALSE;
}


static void add_fixed_text_style(StyleSheet *ss, struct fixed_text_style *st)
{
	int n = ss->n_fixed_text_styles;
	ss->fixed_text_styles = realloc(ss->fixed_text_styles,
	                                (n+1)*sizeof(st));

	ss->fixed_text_styles[n] = st;

	ss->n_fixed_text_styles = n+1;
}


static struct fixed_text_style *new_fixed_text_style(const char *name)
{
	struct fixed_text_style *st;

	st = malloc(sizeof(*st));
	if ( st == NULL ) return NULL;

	st->name = strdup(name);

	return st;
}


static void add_text_style(StyleSheet *ss, struct text_style *st)
{
	int n = ss->n_text_styles;
	ss->text_styles = realloc(ss->text_styles, (n+1)*sizeof(st));

	ss->text_styles[n] = st;

	ss->n_text_styles = n+1;
}


static struct text_style *new_text_style(const char *name)
{
	struct text_style *st;

	st = malloc(sizeof(*st));
	if ( st == NULL ) return NULL;

	st->name = strdup(name);

	return st;
}


static void default_stylesheet(StyleSheet *ss)
{
	struct fixed_text_style *st;
	struct text_style *nst;

	st = new_fixed_text_style("Slide title");
	st->font = strdup("Sans 40");
	add_fixed_text_style(ss, st);

	st = new_fixed_text_style("Presentation title");
	st->font = strdup("Sans 50");
	add_fixed_text_style(ss, st);

	st = new_fixed_text_style("Presentation author");
	st->font = strdup("Sans 30");
	add_fixed_text_style(ss, st);

	nst = new_text_style("Running text");
	nst->font = strdup("Sans 14");
	add_text_style(ss, nst);
}


StyleSheet *new_stylesheet()
{
	StyleSheet *ss;

	ss = calloc(1, sizeof(struct _stylesheet));
	if ( ss == NULL ) return NULL;

	ss->n_text_styles = 0;
	ss->text_styles = NULL;
	ss->n_fixed_text_styles = 0;
	ss->fixed_text_styles = NULL;
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
	s->cur_fixed_style = NULL;

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
	                         gtk_label_new("Fixed text"));
	do_fixed_text(s, text_box);

	text_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(text_box), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), text_box,
	                         gtk_label_new("Text"));
	do_text(s, text_box);

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
