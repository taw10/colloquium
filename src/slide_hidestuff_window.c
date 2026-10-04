/*
 * slide_hidestuff_window.c
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

#include "slide.h"
#include "slide_hidestuff_window.h"


G_DEFINE_FINAL_TYPE(SlideHideStuffWindow, colloquium_slide_hidestuff_window, GTK_TYPE_WINDOW)

static void colloquium_slide_hidestuff_window_class_init(SlideHideStuffWindowClass *klass)
{
}


static void colloquium_slide_hidestuff_window_init(SlideHideStuffWindow *sw)
{
}


static void close_sig(GtkWidget *widget, SlideHideStuffWindow *hw)
{
    gtk_window_close(GTK_WINDOW(hw));
}


SlideHideStuffWindow *slide_hidestuff_window_new(Slide *s, Narrative *n)
{
    SlideHideStuffWindow *sr;
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *button;
    GtkWidget *label;
    GtkWidget *textview;

    sr = g_object_new(COLLOQUIUM_TYPE_SLIDE_HIDESTUFFWINDOW, NULL);
    gtk_window_set_title(GTK_WINDOW(sr), _("Hide items"));
    gtk_window_set_default_size(GTK_WINDOW(sr), 320, 320);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_window_set_child(GTK_WINDOW(sr), box);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    label = gtk_label_new(_("Enter the SVG object IDs to hide"));
    gtk_box_append(GTK_BOX(box), label);
    label = gtk_label_new(_("Commas or newlines separate entries"));
    gtk_box_append(GTK_BOX(box), label);

    textview = gtk_text_view_new();
    gtk_widget_set_vexpand(GTK_WIDGET(textview), TRUE);
    gtk_box_append(GTK_BOX(box), textview);
    gtk_widget_set_margin_start(textview, 8);
    gtk_widget_set_margin_end(textview, 8);
    gtk_widget_set_margin_top(textview, 8);
    gtk_widget_set_margin_bottom(textview, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_widget_set_halign(hbox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 8);

    button = gtk_button_new_with_label(_("Close"));
    gtk_box_prepend(GTK_BOX(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_sig), sr);

    button = gtk_button_new_with_label(_("Apply"));
    gtk_box_prepend(GTK_BOX(hbox), button);

    return sr;
}
