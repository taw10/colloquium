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


static void sr_destroy_sig(GtkWidget *self, NarrativeWindow *nw)
{
    printf("slide sorter closed for real\n");
    nw->slide_sorter = NULL;
}


static GSList *files_in_narrative(Narrative *n)
{
    GSList *list = NULL;
    GtkTextIter iter;
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(n->textbuf);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "slide");
    gboolean more;

    gtk_text_buffer_get_start_iter(n->textbuf, &iter);
    do {
        GtkTextChildAnchor *anc;
        more = gtk_text_iter_forward_to_tag_toggle(&iter, tag);
        anc = gtk_text_iter_get_child_anchor(&iter);
        if ( anc != NULL ) {
            Slide *slide;
            guint nc;
            GtkWidget **th = gtk_text_child_anchor_get_widgets(anc, &nc);
            assert(nc == 1);
            slide = thumbnail_get_slide(COLLOQUIUM_THUMBNAIL(th[0]));
            if ( g_slist_find_custom(list, slide->ext_filename, (GCompareFunc)g_strcmp0) == NULL ) {
                list = g_slist_prepend(list, slide->ext_filename);
            }
            g_free(th);
        }
    } while ( more );
    return list;
}


static void addfile(gpointer sv, gpointer vp)
{
    char *filename = sv;
    GtkWidget *flowbox = vp;
    GFile *file;
    PopplerDocument *doc;
    int np;
    int i;

    file = g_file_new_for_path(filename);
    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return;

    np = poppler_document_get_n_pages(doc);

    for ( i=0; i<np;  i++ ) {

        Slide *s;
        GtkWidget *th;

        s = slide_new();
        slide_set_ext_filename(s, filename);
        slide_set_ext_number(s, i+1);
        th = thumbnail_new(s, NULL);
        gtk_flow_box_append(GTK_FLOW_BOX(flowbox), GTK_WIDGET(th));

    }

    g_object_unref(doc);
    g_object_unref(file);
}


SlideSorter *slide_sorter_new(NarrativeWindow *nw)
{
    SlideSorter *sr;
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *scroll;

    sr = g_object_new(COLLOQUIUM_TYPE_SLIDE_SORTER, NULL);
    sr->parent = nw;
    sr->flowbox = gtk_flow_box_new();

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(sr), vbox);

    label = gtk_label_new(_("This is a temporary staging area.  Contents are not saved."));
    gtk_box_append(GTK_BOX(vbox), label);

    scroll = gtk_scrolled_window_new();
    gtk_box_append(GTK_BOX(vbox), scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(sr->flowbox));
    gtk_widget_set_vexpand(GTK_WIDGET(scroll), TRUE);

    /* Scan the current state of the narrative, and pull out all the
     * filenames being referred to. */
    sr->source_files = files_in_narrative(nw->n);
    g_slist_foreach(sr->source_files, addfile, sr->flowbox);

    gtk_window_set_hide_on_close(GTK_WINDOW(sr), TRUE);
    g_signal_connect(G_OBJECT(nw), "destroy", G_CALLBACK(sr_destroy_sig), nw);

    return sr;
}
