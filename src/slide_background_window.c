/*
 * slide_background_window.c
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
#include "slide_background_window.h"


G_DEFINE_FINAL_TYPE(SlideBgWindow, colloquium_slide_bg_window, GTK_TYPE_WINDOW)

const char *nobgfile = "resource:///uk/me/bitwiz/colloquium/backgrounds/none.svg";

static void colloquium_slide_bg_window_class_init(SlideBgWindowClass *klass)
{
}


static void colloquium_slide_bg_window_init(SlideBgWindow *sw)
{
}


static int is_no_bg(GFile *bg)
{
    GFile *tmp = g_file_new_for_uri(nobgfile);
    if ( g_file_equal(bg, tmp) ) {
        g_object_unref(tmp);
        return 1;
    }
    g_object_unref(tmp);
    return 0;
}


static void activate_sig(GtkListView  *list, guint pos, gpointer vp)
{
    Slide *s = vp;
    GFile *bg = g_list_model_get_item(G_LIST_MODEL(gtk_list_view_get_model(list)), pos);
    if ( s->background != NULL ) {
        g_object_unref(s->background);
    }
    if ( is_no_bg(bg) ) {
        s->background = NULL;
    } else {
        s->background = g_file_dup(bg);
    }
}


static void setup_sig(GtkListItemFactory *factory, GtkListItem *item)
{
    GtkWidget *box;
    GtkWidget *image;
    GtkWidget *label;

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    image = gtk_picture_new();
    gtk_box_append(GTK_BOX(box), image);

    label = gtk_label_new("");
    gtk_box_append(GTK_BOX(box), label);

    gtk_list_item_set_child(item, box);

    gtk_widget_set_size_request(image, 128, 96);
    gtk_picture_set_content_fit(GTK_PICTURE(image), GTK_CONTENT_FIT_CONTAIN);
}


static void bind_sig(GtkListItemFactory *factory, GtkListItem *item)
{
    GtkWidget *image;
    GtkWidget *label;
    GFile *file;

    image = gtk_widget_get_first_child(gtk_list_item_get_child(item));
    label = gtk_widget_get_next_sibling(image);
    file = gtk_list_item_get_item(item);

    if ( is_no_bg(file) ) {
        gtk_label_set_label(GTK_LABEL(label), "No background");
    } else {
        char *bn = g_file_get_basename(file);
        gtk_label_set_label(GTK_LABEL(label), bn);
        g_free(bn);
    }

    gtk_picture_set_file(GTK_PICTURE(image), file);
}


static gboolean drop_sig(GtkDropTarget *drop, const GValue *value,
                         double x, double y, gpointer vp)
{
    GListStore *store = vp;

    if ( G_VALUE_HOLDS(value, G_TYPE_FILE) ) {
        GFile *file = G_FILE(g_value_get_object(value));
        g_list_store_append(store, g_file_dup(file));
        return TRUE;
    } else {
        return FALSE;
    }
}


SlideBgWindow *slide_background_window_new(Slide *s, Narrative *n)
{
    SlideBgWindow *sr;
    GtkWidget *list;
    GtkWidget *scroll;
    GListStore *store;
    GtkListItemFactory *factory;
    GtkDropTarget *drop;
    GType drop_types[1] = { G_TYPE_FILE };

    sr = g_object_new(COLLOQUIUM_TYPE_SLIDE_BGWINDOW, NULL);
    gtk_window_set_title(GTK_WINDOW(sr), _("Select background for slide"));
    gtk_window_set_default_size(GTK_WINDOW(sr), 256, 480);

    factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_sig), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_sig), NULL);

    scroll = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(sr), scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    store = g_list_store_new(G_TYPE_FILE);
    g_list_store_append(store, g_file_new_for_uri(nobgfile));

    if ( n != NULL ) {
        int i;
        for ( i=0; i<n->n_slides; i++ ) {
            if ( n->slides[i]->background != NULL ) {
                if ( !g_list_store_find_with_equal_func(store, G_OBJECT(n->slides[i]->background),
                                                        (GEqualFunc)g_file_equal, NULL) ) {
                    g_list_store_append(store, g_file_dup(n->slides[i]->background));
                }
            }
        }
    }

    list = gtk_list_view_new(GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(store))),
            factory);
    g_signal_connect(list, "activate", G_CALLBACK(activate_sig), s);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(list));

    drop = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY);
    gtk_drop_target_set_gtypes(drop, drop_types, 1);

    g_signal_connect(drop, "drop", G_CALLBACK(drop_sig), store);
    gtk_widget_add_controller(GTK_WIDGET(sr), GTK_EVENT_CONTROLLER(drop));

    return sr;
}
