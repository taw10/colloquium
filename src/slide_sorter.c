/*
 * slide_sorter.c
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
#include <gdk/gdkkeysyms.h>
#include <poppler.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "slide.h"
#include "slide_sorter.h"
#include "thumbnailwidget.h"


G_DEFINE_FINAL_TYPE(SlideSorter, colloquium_slide_sorter, GTK_TYPE_WINDOW)


static void colloquium_slide_sorter_class_init(SlideSorterClass *klass)
{
}


static void colloquium_slide_sorter_init(SlideSorter *sw)
{
}


static void addfile(SlideSorter *ss, GFile *file)
{
    PopplerDocument *doc;
    int np;
    int i;

    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return;

    np = poppler_document_get_n_pages(doc);

    for ( i=0; i<np;  i++ ) {

        Slide *s;
        GtkWidget *th;

        s = slide_new();
        slide_set_ext_file(s, file);
        slide_set_ext_number(s, i+1);
        th = thumbnail_new(s, NULL);
        gtk_widget_set_size_request(GTK_WIDGET(th), 128, -1);
        gtk_flow_box_append(GTK_FLOW_BOX(ss->flowbox), GTK_WIDGET(th));

    }

    g_object_unref(doc);
}


SlideSorter *slide_sorter_new(GFile *file)
{
    SlideSorter *sr;
    GtkWidget *vbox;
    GtkWidget *scroll;

    sr = g_object_new(COLLOQUIUM_TYPE_SLIDE_SORTER, NULL);
    sr->flowbox = gtk_flow_box_new();

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(sr), vbox);

    scroll = gtk_scrolled_window_new();
    gtk_box_append(GTK_BOX(vbox), scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(sr->flowbox));
    gtk_widget_set_vexpand(GTK_WIDGET(scroll), TRUE);

    addfile(sr, file);
    sr->source_file = file;
    g_object_ref(file);

    gtk_window_set_hide_on_close(GTK_WINDOW(sr), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(sr), 512, 768);

    return sr;
}
