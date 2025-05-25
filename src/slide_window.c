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
#include "slideview.h"
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
    g_signal_new("laser-on", GTK_TYPE_SLIDE_WINDOW,
                 G_SIGNAL_RUN_LAST, 0,
                 NULL, NULL, NULL, G_TYPE_NONE, 0);
    g_signal_new("laser-off", GTK_TYPE_SLIDE_WINDOW,
                 G_SIGNAL_RUN_LAST, 0,
                 NULL, NULL, NULL, G_TYPE_NONE, 0);
    g_signal_new("laser-moved", GTK_TYPE_SLIDE_WINDOW,
                 G_SIGNAL_RUN_LAST, 0,
                 NULL, NULL, NULL, G_TYPE_NONE, 2,
                 G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}


static void gtk_slide_window_init(SlideWindow *sw)
{
}


static void slide_click_sig(GtkGestureClick *self, int n_press,
                            gdouble x, gdouble y, gpointer vp)
{
    SlideWindow *sw = vp;

    if ( n_press != 2 ) return;
    if ( gtk_window_is_fullscreen(GTK_WINDOW(sw)) ) {
        gtk_window_unfullscreen(GTK_WINDOW(sw));
        gtk_widget_set_cursor_from_name(GTK_WIDGET(sw->sv), "default");
    } else {
        gtk_window_fullscreen(GTK_WINDOW(sw));
        gtk_widget_set_cursor_from_name(GTK_WIDGET(sw->sv), "none");
    }
}


static gboolean slide_key_press_sig(GtkEventControllerKey *self,
                                    guint keyval,
                                    guint keycode,
                                    GdkModifierType state,
                                    SlideWindow *sw)
{
    switch ( keyval ) {

        case GDK_KEY_Escape :
        if ( gtk_window_is_fullscreen(GTK_WINDOW(sw)) ) {
            gtk_window_unfullscreen(GTK_WINDOW(sw));
            gtk_widget_set_cursor_from_name(GTK_WIDGET(sw->sv), "default");
        }
        break;
    }

    return FALSE;
}

void slide_window_set_laser(SlideWindow *sw, double x, double y)
{
    slide_view_set_laser(COLLOQUIUM_SLIDE_VIEW(sw->sv), x, y);
}


void slide_window_set_laser_off(SlideWindow *sw)
{
    slide_view_set_laser_off(COLLOQUIUM_SLIDE_VIEW(sw->sv));
}


static void slide_leave_sig(GtkEventControllerMotion *self, SlideWindow *sw)
{
    if ( sw->laser_on ) {
        g_signal_emit_by_name(G_OBJECT(sw), "laser-off");
        sw->laser_on = 0;
    }
}


static void slide_motion_sig(GtkEventControllerMotion *self,
                             gdouble x, gdouble y, SlideWindow *sw)
{
    if ( !sw->laser_on ) {
        g_signal_emit_by_name(G_OBJECT(sw), "laser-on");
        sw->laser_on = 1;
    }
    slide_view_widget_to_relative_coords(COLLOQUIUM_SLIDE_VIEW(sw->sv), &x, &y);
    g_signal_emit_by_name(G_OBJECT(sw), "laser-moved", x, y);
}


SlideWindow *slide_window_new(Narrative *n, Slide *slide,
                              NarrativeWindow *nw, GApplication *papp)
{
    SlideWindow *sw;
    double w, h, asp;
    Colloquium *app = COLLOQUIUM(papp);

    sw = g_object_new(GTK_TYPE_SLIDE_WINDOW, "application", app, NULL);

    sw->n = n;
    sw->slide = slide;
    sw->parent = nw;

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(sw), FALSE);

    slide_window_update_titlebar(sw);

    g_action_map_add_action_entries(G_ACTION_MAP(sw), sw_entries,
                                    G_N_ELEMENTS(sw_entries), sw);

    sw->sv = slide_view_new(n, slide);

    asp = slide_get_aspect(slide);
    if ( asp > 1.0 ) {
        w = 960;
        h = w/slide_get_aspect(slide);
    } else {
        h = 960;
        w = h*slide_get_aspect(slide);
    }
    gtk_window_set_default_size(GTK_WINDOW(sw), w, h);

    gtk_window_set_child(GTK_WINDOW(sw), GTK_WIDGET(sw->sv));

    gtk_window_set_resizable(GTK_WINDOW(sw), TRUE);

    GtkGesture *evc = gtk_gesture_click_new();
    gtk_widget_add_controller(GTK_WIDGET(sw->sv), GTK_EVENT_CONTROLLER(evc));
    g_signal_connect(G_OBJECT(evc), "pressed", G_CALLBACK(slide_click_sig), sw);

    GtkEventController *evk = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(sw), evk);
    g_signal_connect(G_OBJECT(evk), "key-pressed", G_CALLBACK(slide_key_press_sig), sw);

    GtkEventController *evm = gtk_event_controller_motion_new();
    gtk_widget_add_controller(GTK_WIDGET(sw->sv), evm);
    g_signal_connect(G_OBJECT(evm), "motion", G_CALLBACK(slide_motion_sig), sw);
    g_signal_connect(G_OBJECT(evm), "leave", G_CALLBACK(slide_leave_sig), sw);

    return sw;
}


void slide_window_set_slide(SlideWindow *sw, Slide *s)
{
    slide_view_set_slide(sw->sv, s);
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
