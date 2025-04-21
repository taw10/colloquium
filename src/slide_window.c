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


static void sw_about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    SlideWindow *sw = vp;
    open_about_dialog(GTK_WIDGET(sw));
}

GActionEntry sw_entries[] = {

    { "about", sw_about_sig, NULL, NULL, NULL },
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
    Colloquium *app = COLLOQUIUM(papp);

    sw = g_object_new(GTK_TYPE_SLIDE_WINDOW, "application", app, NULL);

    sw->n = n;
    sw->slide = slide;
    sw->parent = nw;

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(sw), FALSE);

    slide_window_update_titlebar(sw);

    g_action_map_add_action_entries(G_ACTION_MAP(sw), sw_entries,
                                    G_N_ELEMENTS(sw_entries), sw);

    sw->sv = gtk_slide_view_new(n, slide);

    w = 1280;
    h = w/slide_get_aspect(slide);
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
    snprintf(title, 1024, "%s - Colloquium", filename);
    if ( gtk_text_buffer_get_modified(sw->n->textbuf) ) strcat(title, " *");

    gtk_window_set_title(GTK_WINDOW(sw), title);
}
