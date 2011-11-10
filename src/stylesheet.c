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

#include "presentation.h"
#include "stylesheet.h"
#include "objects.h"
#include "loadsave.h"


struct _stylesheetwindow
{
	struct presentation *p;  /* Presentation to update when user alters
	                          * something in this window */
	GtkWidget           *window;
	StyleSheet          *ss; /* Style sheet this window corresponds to */

	GtkWidget           *text_font;
	GtkWidget           *text_colour;

	GtkWidget           *margin_left;
	GtkWidget           *margin_right;
	GtkWidget           *margin_top;
	GtkWidget           *margin_bottom;
	GtkWidget           *offset_x;
	GtkWidget           *offset_y;
	GtkWidget           *halign;
	GtkWidget           *valign;
	GtkWidget           *max_width;
	GtkWidget           *use_max;

	struct style        *cur_style;
};


static void text_font_set_sig(GtkFontButton *widget,
                              struct _stylesheetwindow *s)
{
	const gchar *font;

	font = gtk_font_button_get_font_name(widget);
	free(s->cur_style->font);
	s->cur_style->font = strdup(font);

	notify_style_update(s->p, s->cur_style);
}


static void text_colour_set_sig(GtkColorButton *widget,
                              struct _stylesheetwindow *s)
{
	GdkColor col;
	guint16 al;

	gtk_color_button_get_color(widget, &col);
	free(s->cur_style->colour);
	s->cur_style->colour = gdk_color_to_string(&col);
	al = gtk_color_button_get_alpha(widget);
	s->cur_style->alpha = (double)al / 65535.0;

	notify_style_update(s->p, s->cur_style);
}


static void margin_left_changed_sig(GtkSpinButton *spin,
                                    struct _stylesheetwindow *s)
{
	s->cur_style->margin_left = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void margin_right_changed_sig(GtkSpinButton *spin,
                                     struct _stylesheetwindow *s)
{
	s->cur_style->margin_right = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void margin_top_changed_sig(GtkSpinButton *spin,
                                   struct _stylesheetwindow *s)
{
	s->cur_style->margin_top = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void margin_bottom_changed_sig(GtkSpinButton *spin,
                                      struct _stylesheetwindow *s)
{
	s->cur_style->margin_bottom = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void offset_x_changed_sig(GtkSpinButton *spin,
                                 struct _stylesheetwindow *s)
{
	s->cur_style->offset_x = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void offset_y_changed_sig(GtkSpinButton *spin,
                                 struct _stylesheetwindow *s)
{
	s->cur_style->offset_y = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void halign_changed_sig(GtkComboBox *combo,
                                 struct _stylesheetwindow *s)
{
	s->cur_style->halign = gtk_combo_box_get_active(combo);
	notify_style_update(s->p, s->cur_style);
}


static void valign_changed_sig(GtkComboBox *combo,
                                 struct _stylesheetwindow *s)
{
	s->cur_style->valign = gtk_combo_box_get_active(combo);
	notify_style_update(s->p, s->cur_style);
}


static void max_changed_sig(GtkSpinButton *spin,
                            struct _stylesheetwindow *s)
{
	s->cur_style->max_width = gtk_spin_button_get_value(spin);
	notify_style_update(s->p, s->cur_style);
}


static void use_max_toggled_sig(GtkToggleButton *combo,
                                 struct _stylesheetwindow *s)
{
	int v;

	v = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->use_max));
	s->cur_style->use_max_width = v;
	gtk_widget_set_sensitive(s->max_width,
	                         s->cur_style->use_max_width);
	notify_style_update(s->p, s->cur_style);
}


static void style_changed_sig(GtkComboBox *combo,
                               struct _stylesheetwindow *s)
{
	int n;
	GdkColor col;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	s->cur_style = s->ss->styles[n];

	if ( n == 0 ) {
		gtk_widget_set_sensitive(s->offset_x, FALSE);
		gtk_widget_set_sensitive(s->offset_y, FALSE);
		gtk_widget_set_sensitive(s->use_max, FALSE);
		gtk_widget_set_sensitive(s->max_width, FALSE);
		gtk_widget_set_sensitive(s->halign, FALSE);
		gtk_widget_set_sensitive(s->valign, FALSE);
	} else {
		gtk_widget_set_sensitive(s->offset_x, TRUE);
		gtk_widget_set_sensitive(s->offset_y, TRUE);
		gtk_widget_set_sensitive(s->use_max, TRUE);
		gtk_widget_set_sensitive(s->max_width, TRUE);
		gtk_widget_set_sensitive(s->halign, TRUE);
		gtk_widget_set_sensitive(s->valign, TRUE);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_left),
	                          s->cur_style->margin_left);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_right),
	                          s->cur_style->margin_right);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_bottom),
	                          s->cur_style->margin_bottom);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->margin_top),
	                          s->cur_style->margin_top);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->offset_x),
	                          s->cur_style->offset_x);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->offset_y),
	                          s->cur_style->offset_y);

	gtk_combo_box_set_active(GTK_COMBO_BOX(s->halign),
	                         s->cur_style->halign);
	gtk_combo_box_set_active(GTK_COMBO_BOX(s->valign),
	                         s->cur_style->valign);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->use_max),
	                             s->cur_style->use_max_width);
	gtk_widget_set_sensitive(s->max_width, s->cur_style->use_max_width);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->max_width),
	                          s->cur_style->max_width);

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	s->cur_style = s->ss->styles[n];

	gtk_font_button_set_font_name(GTK_FONT_BUTTON(s->text_font),
	                              s->cur_style->font);

	gdk_color_parse(s->cur_style->colour, &col);
	gtk_color_button_set_color(GTK_COLOR_BUTTON(s->text_colour), &col);
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
	label = gtk_label_new("Style:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_new_text();
	for ( i=0; i<s->ss->n_styles; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          s->ss->styles[i]->name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
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
	s->offset_y = gtk_spin_button_new_with_range(-s->p->slide_height,
	                                             +s->p->slide_height, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), s->offset_y, 1, 2, 0, 1);
	g_signal_connect(G_OBJECT(s->offset_y), "value-changed",
	                 G_CALLBACK(offset_y_changed_sig), s);

	/* Right */
	label = gtk_label_new("Across:");
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	s->offset_x = gtk_spin_button_new_with_range(-s->p->slide_width,
	                                             +s->p->slide_width, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(table), s->offset_x, 1, 2, 1, 2);
	g_signal_connect(G_OBJECT(s->offset_x), "value-changed",
	                 G_CALLBACK(offset_x_changed_sig), s);

	table = gtk_table_new(3, 2, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5.0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5.0);
	gtk_box_pack_start(GTK_BOX(b), table, FALSE, FALSE, 5);
	label = gtk_label_new("Horizontal alignment:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	s->halign = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), s->halign, 1, 2, 0, 1);
	gtk_combo_box_append_text(GTK_COMBO_BOX(s->halign), "Left");
	gtk_combo_box_append_text(GTK_COMBO_BOX(s->halign), "Centre");
	gtk_combo_box_append_text(GTK_COMBO_BOX(s->halign), "Right");
	g_signal_connect(G_OBJECT(s->halign), "changed",
	                 G_CALLBACK(halign_changed_sig), s);

	label = gtk_label_new("Vertical alignment:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	s->valign = gtk_combo_box_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), s->valign, 1, 2, 1, 2);
	gtk_combo_box_append_text(GTK_COMBO_BOX(s->valign), "Top");
	gtk_combo_box_append_text(GTK_COMBO_BOX(s->valign), "Centre");
	gtk_combo_box_append_text(GTK_COMBO_BOX(s->valign), "Bottom");
	g_signal_connect(G_OBJECT(s->valign), "changed",
	                 G_CALLBACK(valign_changed_sig), s);

	label = gtk_label_new("Maximum width:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	box = gtk_hbox_new(FALSE, 5.0);
	gtk_table_attach_defaults(GTK_TABLE(table), box, 1, 2, 2, 3);
	s->use_max = gtk_check_button_new();
	gtk_box_pack_start(GTK_BOX(box), s->use_max, FALSE, FALSE, 0);
	s->max_width = gtk_spin_button_new_with_range(0.0, 1024.0, 1.0);
	gtk_box_pack_start(GTK_BOX(box), s->max_width, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(s->use_max), "toggled",
	                 G_CALLBACK(use_max_toggled_sig), s);
	g_signal_connect(G_OBJECT(s->max_width), "value-changed",
	                 G_CALLBACK(max_changed_sig), s);

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
	style_changed_sig(GTK_COMBO_BOX(combo), s);
}



static gint destroy_stylesheet_sig(GtkWidget *w, struct _stylesheetwindow *s)
{
	s->p->stylesheetwindow = NULL;
	free(s);
	return FALSE;
}


struct style *new_style(StyleSheet *ss, const char *name)
{
	struct style *sty;
	int n;
	struct style **styles_new;

	sty = calloc(1, sizeof(*sty));
	if ( sty == NULL ) return NULL;

	sty->name = strdup(name);

	n = ss->n_styles;
	styles_new = realloc(ss->styles, (n+1)*sizeof(sty));
	if ( styles_new == NULL ) {
		free(sty->name);
		free(sty);
		return NULL;
	}
	ss->styles = styles_new;
	ss->styles[n] = sty;
	ss->n_styles = n+1;

	return sty;
}


void free_stylesheet(StyleSheet *ss)
{
	int i;

	for ( i=0; i<ss->n_styles; i++ ) {
		free(ss->styles[i]->name);
		free(ss->styles[i]);
	}

	free(ss->styles);
	free(ss);
}


void default_stylesheet(StyleSheet *ss)
{
	struct style *sty;

	/* Default style must be first */
	sty = new_style(ss, "Default");
	sty->font = strdup("Sans 18");
	sty->colour = strdup("#000000000000");  /* Black */
	sty->alpha = 1.0;
	sty->margin_left = 20.0;
	sty->margin_right = 20.0;
	sty->margin_top = 20.0;
	sty->margin_bottom = 20.0;
	sty->halign = J_CENTER;  /* Ignored */
	sty->valign = V_CENTER;  /* Ignored */
	sty->offset_x = 0.0;  /* Ignored */
	sty->offset_y = 0.0;  /* Ignored */

	sty = new_style(ss, "Slide title");
	sty->font = strdup("Sans 40");
	sty->colour = strdup("#000000000000");  /* Black */
	sty->alpha = 1.0;
	sty->margin_left = 20.0;
	sty->margin_right = 20.0;
	sty->margin_top = 20.0;
	sty->margin_bottom = 20.0;
	sty->halign = J_CENTER;
	sty->valign = V_TOP;
	sty->offset_x = 0.0;
	sty->offset_y = 0.0;  /* irrelevant */

	sty = new_style(ss, "Slide credit");
	sty->font = strdup("Sans 14");
	sty->colour = strdup("#000000000000");  /* Black */
	sty->alpha = 1.0;
	sty->margin_left = 20.0;
	sty->margin_right = 20.0;
	sty->margin_top = 20.0;
	sty->margin_bottom = 35.0;
	sty->halign = J_RIGHT;
	sty->valign = V_BOTTOM;
	sty->offset_x = 0.0;
	sty->offset_y = 0.0;

	sty = new_style(ss, "Slide date");
	sty->font = strdup("Sans 12");
	sty->colour = strdup("#999999999999");  /* Grey */
	sty->alpha = 1.0;
	sty->margin_left = 600.0;
	sty->margin_right = 100.0;
	sty->margin_top = 745.0;
	sty->margin_bottom = 5.0;
	sty->halign = J_RIGHT;
	sty->valign = V_BOTTOM;
	sty->offset_x = 0.0;
	sty->offset_y = 0.0;

	sty = new_style(ss, "Slide number");
	sty->font = strdup("Sans 12");
	sty->colour = strdup("#999999999999");  /* Grey */
	sty->alpha = 1.0;
	sty->margin_left = 600.0;
	sty->margin_right = 5.0;
	sty->margin_top = 745.0;
	sty->margin_bottom = 5.0;
	sty->halign = J_RIGHT;
	sty->valign = V_BOTTOM;
	sty->offset_x = 0.0;
	sty->offset_y = 0.0;

	sty = new_style(ss, "Presentation title on slide");
	sty->font = strdup("Sans 12");
	sty->colour = strdup("#999999999999");  /* Grey */
	sty->alpha = 1.0;
	sty->margin_left = 5.0;
	sty->margin_right = 600.0;
	sty->margin_top = 745.0;
	sty->margin_bottom = 5.0;
	sty->halign = J_LEFT;
	sty->valign = V_BOTTOM;
	sty->offset_x = 0.0;
	sty->offset_y = 0.0;

	sty = new_style(ss, "Presentation title");
	sty->font = strdup("Sans 50");
	sty->colour = strdup("#000000000000");  /* Black */
	sty->alpha = 1.0;
	sty->margin_left = 20.0;
	sty->margin_right = 20.0;
	sty->margin_top = 20.0;
	sty->margin_bottom = 20.0;
	sty->halign = J_CENTER;
	sty->valign = V_CENTER;
	sty->offset_x = -200.0;
	sty->offset_y = +300.0;

	sty = new_style(ss, "Presentation author");
	sty->font = strdup("Sans 30");
	sty->colour = strdup("#000000000000");  /* Black */
	sty->alpha = 1.0;
	sty->margin_left = 20.0;
	sty->margin_right = 20.0;
	sty->margin_top = 20.0;
	sty->margin_bottom = 20.0;
	sty->halign = J_CENTER;
	sty->valign = V_CENTER;
	sty->offset_x = +200.0;
	sty->offset_y = -300.0;

	sty = new_style(ss, "Presentation date");
	sty->font = strdup("Sans 30");
	sty->colour = strdup("#000000000000");  /* Black */
	sty->alpha = 1.0;
	sty->margin_left = 20.0;
	sty->margin_right = 20.0;
	sty->margin_top = 20.0;
	sty->margin_bottom = 20.0;
	sty->halign = J_CENTER;
	sty->valign = V_CENTER;
	sty->offset_x = +200.0;
	sty->offset_y = -300.0;

	ss->bgblocks = malloc(sizeof(struct bgblock));
	ss->n_bgblocks = 1;
	ss->bgblocks[0].type = BGBLOCK_SOLID;
	ss->bgblocks[0].min_x = 0.0;
	ss->bgblocks[0].max_x = 1024.0;
	ss->bgblocks[0].min_y = 0.0;
	ss->bgblocks[0].max_y = 768.0;
	ss->bgblocks[0].colour1 = strdup("#ffffffffffff");
	ss->bgblocks[0].alpha1 = 1.0;

}


static const char *str_halign(enum justify halign)
{
	switch ( halign ) {
		case J_LEFT   : return "left";
		case J_RIGHT  : return "right";
		case J_CENTER : return "center";
		default : return "???";
	}
}


enum justify str_to_halign(char *halign)
{
	if ( strcmp(halign, "left") == 0 ) return J_LEFT;
	if ( strcmp(halign, "right") == 0 ) return J_RIGHT;
	if ( strcmp(halign, "center") == 0 ) return J_CENTER;

	return J_LEFT;
}


static const char *str_valign(enum vert_pos valign)
{
	switch ( valign ) {
		case V_TOP    : return "top";
		case V_BOTTOM : return "bottom";
		case V_CENTER : return "center";
		default : return "???";
	}
}


enum vert_pos str_to_valign(char *valign)
{
	if ( strcmp(valign, "top") == 0 ) return V_TOP;
	if ( strcmp(valign, "bottom") == 0 ) return V_BOTTOM;
	if ( strcmp(valign, "center") == 0 ) return V_CENTER;

	return J_LEFT;
}


static const char *str_bgtype(enum bgblocktype t)
{
	switch ( t ) {
		case BGBLOCK_SOLID             : return "solid";
		case BGBLOCK_GRADIENT_X        : return "gradient_x";
		case BGBLOCK_GRADIENT_Y        : return "gradient_y";
		case BGBLOCK_GRADIENT_CIRCULAR : return "gradient_circular";
		case BGBLOCK_IMAGE             : return "image";
		default : return "???";
	}
}


static enum bgblocktype str_to_bgtype(char *t)
{
	if ( strcmp(t, "solid") == 0 ) return BGBLOCK_SOLID;
	if ( strcmp(t, "gradient_x") == 0 ) return BGBLOCK_GRADIENT_X;
	if ( strcmp(t, "gradient_y") == 0 ) return BGBLOCK_GRADIENT_Y;
	if ( strcmp(t, "gradient_ciruclar") == 0 ) {
		return BGBLOCK_GRADIENT_CIRCULAR;
	}
	if ( strcmp(t, "image") == 0 ) return BGBLOCK_IMAGE;

	return BGBLOCK_SOLID;
}


static int read_style(struct style *sty, struct ds_node *root)
{
	char *align;

	get_field_f(root, "margin_left",   &sty->margin_left);
	get_field_f(root, "margin_right",  &sty->margin_right);
	get_field_f(root, "margin_top",    &sty->margin_top);
	get_field_f(root, "margin_bottom", &sty->margin_bottom);

	get_field_i(root, "use_max_width", &sty->use_max_width);
	get_field_f(root, "max_width",     &sty->max_width);

	get_field_f(root, "offset_x",      &sty->offset_x);
	get_field_f(root, "offset_y",      &sty->offset_y);

	get_field_s(root, "font",          &sty->font);
	get_field_s(root, "colour",        &sty->colour);
	get_field_f(root, "alpha",         &sty->alpha);

	get_field_s(root, "halign",        &align);
	sty->halign = str_to_halign(align);
	free(align);
	get_field_s(root, "valign",        &align);
	sty->valign = str_to_valign(align);
	free(align);

	return 0;
}


static int read_bgblock(struct bgblock *b, struct ds_node *root)
{
	char *type;

	get_field_s(root, "type",  &type);
	b->type = str_to_bgtype(type);

	get_field_f(root, "min_x",  &b->min_x);
	get_field_f(root, "max_x",  &b->max_x);
	get_field_f(root, "min_y",  &b->min_y);
	get_field_f(root, "max_y",  &b->max_y);

	switch ( b->type ) {

		case BGBLOCK_SOLID :
		get_field_s(root, "colour1",  &b->colour1);
		get_field_f(root, "alpha1",  &b->alpha1);

		case BGBLOCK_GRADIENT_X :
		case BGBLOCK_GRADIENT_Y :
		case BGBLOCK_GRADIENT_CIRCULAR :
		get_field_s(root, "colour1",  &b->colour1);
		get_field_f(root, "alpha1",  &b->alpha1);
		get_field_s(root, "colour2",  &b->colour2);
		get_field_f(root, "alpha2",  &b->alpha2);

		default:
		break;

	}

	return 0;
}


StyleSheet *tree_to_stylesheet(struct ds_node *root)
{
	StyleSheet *ss;
	struct ds_node *node;
	int i;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

	node = find_node(root, "styles");
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find styles\n");
		free_stylesheet(ss);
		return NULL;
	}

	for ( i=0; i<node->n_children; i++ ) {

		struct style *ns;
		char *v;

		get_field_s(node->children[i], "name", &v);
		if ( v == NULL ) {
			fprintf(stderr, "No name for style '%s'\n",
			        node->children[i]->key);
			continue;
		}

		ns = new_style(ss, v);
		if ( ns == NULL ) {
			fprintf(stderr, "Couldn't create style for '%s'\n",
			        node->children[i]->key);
			continue;
		}

		if ( read_style(ns, node->children[i]) ) {
			fprintf(stderr, "Couldn't read style '%s'\n", v);
			continue;
		}

	}

	node = find_node(root, "bgblocks");
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find bgblocks\n");
		free_stylesheet(ss);
		return NULL;
	}

	ss->bgblocks = malloc(node->n_children * sizeof(struct bgblock));
	if ( ss->bgblocks == NULL ) {
		fprintf(stderr, "Couldn't allocate bgblocks\n");
		free_stylesheet(ss);
		return NULL;
	}
	ss->n_bgblocks = node->n_children;

	for ( i=0; i<node->n_children; i++ ) {

		struct bgblock *b;

		b = &ss->bgblocks[i];

		if ( read_bgblock(b, node->children[i]) ) {
			fprintf(stderr, "Couldn't read bgblock %i\n", i);
			continue;
		}

	}

	return ss;
}


StyleSheet *new_stylesheet()
{
	StyleSheet *ss;

	ss = calloc(1, sizeof(struct _stylesheet));
	if ( ss == NULL ) return NULL;

	ss->n_styles = 0;
	ss->styles = NULL;

	return ss;
}


int save_stylesheet(StyleSheet *ss, const char *filename)
{
	FILE *fh;
	struct serializer ser;

	fh = fopen(filename, "w");
	if ( fh == NULL ) return 1;

	/* Set up the serializer */
	ser.fh = fh;
	ser.stack_depth = 0;
	ser.prefix = NULL;

	fprintf(fh, "# Colloquium style sheet file\n");
	serialize_f(&ser, "version", 0.1);

	serialize_start(&ser, "stylesheet");
	write_stylesheet(ss, &ser);
	serialize_end(&ser);

	return 0;
}


StyleSheet *load_stylesheet(const char *filename)
{
	StyleSheet *ss;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

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
	s->cur_style = NULL;
	s->cur_style = NULL;

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
	                         gtk_label_new("Styles"));
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


void write_stylesheet(StyleSheet *ss, struct serializer *ser)
{
	int i;

	serialize_start(ser, "bgblocks");
	for ( i=0; i<ss->n_bgblocks; i++ ) {

		struct bgblock *b = &ss->bgblocks[i];
		char id[32];

		snprintf(id, 31, "%i", i);

		serialize_start(ser, id);
		serialize_s(ser, "type", str_bgtype(b->type));
		serialize_f(ser, "min_x", b->min_x);
		serialize_f(ser, "min_y", b->min_y);
		serialize_f(ser, "max_x", b->max_x);
		serialize_f(ser, "max_y", b->max_y);

		switch ( b->type ) {

			case BGBLOCK_SOLID :
			serialize_s(ser, "colour1", b->colour1);
			serialize_f(ser, "alpha1", b->alpha1);
			break;

			case BGBLOCK_GRADIENT_X :
			case BGBLOCK_GRADIENT_Y :
			case BGBLOCK_GRADIENT_CIRCULAR :
			serialize_s(ser, "colour1", b->colour1);
			serialize_f(ser, "alpha1", b->alpha1);
			serialize_s(ser, "colour2", b->colour2);
			serialize_f(ser, "alpha2", b->alpha2);
			break;

			default:
			break;

		}

		serialize_end(ser);

	}
	serialize_end(ser);

	serialize_start(ser, "styles");
	for ( i=0; i<ss->n_styles; i++ ) {

		struct style *s = ss->styles[i];
		char id[32];

		snprintf(id, 31, "%i", i);

		serialize_start(ser, id);
		serialize_s(ser, "name", s->name);
		serialize_f(ser, "margin_left", s->margin_left);
		serialize_f(ser, "margin_right", s->margin_right);
		serialize_f(ser, "margin_top", s->margin_top);
		serialize_f(ser, "margin_bottom", s->margin_bottom);
		serialize_b(ser, "use_max_width", s->use_max_width);
		serialize_f(ser, "max_width", s->max_width);
		serialize_f(ser, "offset_x", s->offset_x);
		serialize_f(ser, "offset_y", s->offset_y);
		serialize_s(ser, "font", s->font);
		serialize_s(ser, "colour", s->colour);
		serialize_f(ser, "alpha", s->alpha);
		serialize_s(ser, "halign", str_halign(s->halign));
		serialize_s(ser, "valign", str_valign(s->valign));
		serialize_end(ser);

	}
	serialize_end(ser);
}


struct style *find_style(StyleSheet *ss, const char *name)
{
	int i;
	for ( i=0; i<ss->n_styles; i++ ) {
		if ( strcmp(ss->styles[i]->name, name) == 0 ) {
			return ss->styles[i];
		}
	}

	return NULL;
}
