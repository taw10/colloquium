/*
 * slide_window.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "narrative.h"
#include "slide.h"
#include "gtkslideview.h"
#include "colloquium.h"
#include "slide_window.h"
#include "narrative_window.h"


G_DEFINE_TYPE_WITH_CODE(SlideWindow, gtk_slide_window, GTK_TYPE_APPLICATION_WINDOW, NULL)

/* Change the editor's slide to "np" */
static void change_edit_slide(SlideWindow *sw, Slide *np)
{
    gtk_slide_view_set_slide(sw->sv, np);
    sw->slide = np;
    slide_window_update_titlebar(sw);
}


static void change_slide_first(SlideWindow *sw)
{
    Slide *s = narrative_get_slide_by_number(sw->n, 0);
    if ( s != NULL ) change_edit_slide(sw, s);
}


static void change_slide_backwards(SlideWindow *sw)
{
    int slide_n = narrative_get_slide_number_for_slide(sw->n, sw->slide);
    if ( slide_n > 0 ) {
        Slide *s = narrative_get_slide_by_number(sw->n, slide_n-1);
        change_edit_slide(sw, s);
    }
}


static void change_slide_forwards(SlideWindow *sw)
{
    int slide_n = narrative_get_slide_number_for_slide(sw->n, sw->slide);
    Slide *s = narrative_get_slide_by_number(sw->n, slide_n+1);
    if ( s != NULL ) change_edit_slide(sw, s);
}


static void change_slide_last(SlideWindow *sw)
{
    int slide_n = narrative_get_num_slides(sw->n);
    Slide *s = narrative_get_slide_by_number(sw->n, slide_n);
    if ( s != NULL ) change_edit_slide(sw, s);
}


static void first_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
    SlideWindow *sw = vp;
    change_slide_first(sw);
}


static void prev_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
    SlideWindow *sw = vp;
    change_slide_backwards(sw);
}


static void next_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
    SlideWindow *sw = vp;
    change_slide_forwards(sw);
}


static void last_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
    SlideWindow *sw = vp;
    change_slide_last(sw);
}


static gboolean sw_key_press_sig(GtkEventControllerKey *self,
                                 guint keyval,
                                 guint keycode,
                                 GdkModifierType state,
                                 SlideWindow *sw)
{
    switch ( keyval ) {

        case GDK_KEY_Page_Up :
        change_slide_backwards(sw);
        return TRUE;

        case GDK_KEY_Page_Down :
        change_slide_forwards(sw);
        return TRUE;

    }

    return FALSE;
}


static void sw_about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    SlideWindow *sw = vp;
    open_about_dialog(GTK_WIDGET(sw));
}

GActionEntry sw_entries[] = {

    { "about", sw_about_sig, NULL, NULL, NULL },
    { "first", first_slide_sig, NULL, NULL, NULL },
    { "prev", prev_slide_sig, NULL, NULL, NULL },
    { "next", next_slide_sig, NULL, NULL, NULL },
    { "last", last_slide_sig, NULL, NULL, NULL },
};


static void gtk_slide_window_class_init(SlideWindowClass *klass)
{
}


static void gtk_slide_window_init(SlideWindow *sw)
{
}


SlideWindow *slide_window_new(Narrative *n, Slide *slide,
                              NarrativeWindow *nw, GApplication *papp)
{
    SlideWindow *sw;
    double w, h;
    GtkEventController *evc;
    Colloquium *app = COLLOQUIUM(papp);

    sw = g_object_new(GTK_TYPE_SLIDE_WINDOW, "application", app, NULL);

    sw->n = n;
    sw->slide = slide;
    sw->parent = nw;

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(sw), TRUE);

    slide_window_update_titlebar(sw);

    g_action_map_add_action_entries(G_ACTION_MAP(sw), sw_entries,
                                    G_N_ELEMENTS(sw_entries), sw);

    sw->sv = gtk_slide_view_new(n, slide);

    evc = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(sw), evc);
    g_signal_connect(G_OBJECT(evc), "key-press",
                     G_CALLBACK(sw_key_press_sig), sw);

    slide_get_logical_size(slide, &w, &h);
    gtk_window_set_default_size(GTK_WINDOW(sw), w, h);

    gtk_window_set_child(GTK_WINDOW(sw), GTK_WIDGET(sw->sv));

    gtk_window_set_resizable(GTK_WINDOW(sw), TRUE);

    return sw;
}


void slide_window_update(SlideWindow *sw)
{
    gtk_widget_queue_draw(GTK_WIDGET(sw->sv));
}


void slide_window_update_titlebar(SlideWindow *sw)
{
    char title[1026];
    char *filename;

    filename = narrative_window_get_filename(sw->parent);
    snprintf(title, 1024, "%s (slide %i) - Colloquium", filename,
             1+narrative_get_slide_number_for_slide(sw->n, sw->slide));
    if ( narrative_get_unsaved(sw->n) ) strcat(title, " *");

    gtk_window_set_title(GTK_WINDOW(sw), title);
}
