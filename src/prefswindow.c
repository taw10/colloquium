/*
 * prefswindow.c
 *
 * Copyright © 2025 Thomas White <taw@bitwiz.org.uk>
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
#include <gtk/gtk.h>
#include <assert.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "prefswindow.h"

G_DEFINE_FINAL_TYPE(PrefsWindow, colloquium_prefs_window, GTK_TYPE_WINDOW)


static void colloquium_prefs_window_class_init(PrefsWindowClass *klass)
{
}


static void colloquium_prefs_window_init(PrefsWindow *pw)
{
}


static GtkWidget *narrative_prefs()
{
	GtkWidget *box;
	GtkWidget *hbox;
	GtkFontDialog *fc;
	GtkWidget *fb;

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Font:")));
    fc = gtk_font_dialog_new();
    fb = gtk_font_dialog_button_new(fc);
    gtk_box_append(GTK_BOX(hbox), fb);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Foreground:")));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Background:")));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Margin width:")));

    return box;
}


static GtkWidget *presentation_prefs()
{
	GtkWidget *box;
	GtkWidget *hbox;

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Words per minute:")));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Pointer:")));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Text highlight colour:")));

    return box;
}


PrefsWindow *prefs_window_new()
{
    PrefsWindow *pw;
    GtkWidget *notebook;
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *button;

    pw = g_object_new(COLLOQUIUM_TYPE_PREFS_WINDOW, NULL);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_window_set_child(GTK_WINDOW(pw), box);

    notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(box), notebook);
    gtk_widget_set_vexpand(notebook, TRUE);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         narrative_prefs(),
	                         gtk_label_new(_("Narrative")));

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         presentation_prefs(),
	                         gtk_label_new(_("Presentation")));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_widget_set_halign(hbox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 8);
    gtk_widget_set_margin_bottom(hbox, 8);

    button = gtk_button_new_with_label(_("Close"));
    gtk_box_prepend(GTK_BOX(hbox), button);

    gtk_window_set_default_size(GTK_WINDOW(pw), 480, 320);
    gtk_window_set_title(GTK_WINDOW(pw), _("Preferences"));

    return pw;
}
