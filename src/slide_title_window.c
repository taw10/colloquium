/*
 * slide_title_window.c
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
#include "slide_title_window.h"


G_DEFINE_FINAL_TYPE(SlideTitleWindow, colloquium_slide_title_window, GTK_TYPE_WINDOW)

static void colloquium_slide_title_window_class_init(SlideTitleWindowClass *klass)
{
}


static void colloquium_slide_title_window_init(SlideTitleWindow *sw)
{
}


static void close_sig(GtkWidget *widget, SlideTitleWindow *tw)
{
    gtk_window_close(GTK_WINDOW(tw));
}


static void apply_sig(GtkWidget *widget, SlideTitleWindow *tw)
{
    const char *txt = gtk_editable_get_text(GTK_EDITABLE(tw->entry));
    slide_set_title(tw->s, txt);
}


SlideTitleWindow *slide_title_window_new(Slide *s)
{
    SlideTitleWindow *sr;
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *button;

    sr = g_object_new(COLLOQUIUM_TYPE_SLIDE_TITLE_WINDOW, NULL);
    gtk_window_set_title(GTK_WINDOW(sr), _("Set slide title"));
    gtk_window_set_default_size(GTK_WINDOW(sr), 480, -1);

    sr->s = s;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_window_set_child(GTK_WINDOW(sr), box);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    sr->entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(box), sr->entry);
    gtk_widget_set_margin_start(sr->entry, 8);
    gtk_widget_set_margin_end(sr->entry, 8);
    gtk_widget_set_margin_top(sr->entry, 8);
    gtk_widget_set_margin_bottom(sr->entry, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_widget_set_halign(hbox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 8);

    button = gtk_button_new_with_label(_("Close"));
    gtk_box_prepend(GTK_BOX(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_sig), sr);

    gtk_entry_set_placeholder_text(GTK_ENTRY(sr->entry), _("Slide title"));
    if ( s->title != NULL ) {
        gtk_editable_set_text(GTK_EDITABLE(sr->entry), s->title);
    }

    button = gtk_button_new_with_label(_("Apply"));
    gtk_box_prepend(GTK_BOX(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(apply_sig), sr);

    return sr;
}
