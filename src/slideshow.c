/*
 * slideshow.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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
#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libintl.h>
#define _(x) gettext(x)

#include <narrative.h>

#include "slide_render_cairo.h"
#include "slideshow.h"
#include "colloquium.h"
#include "pr_clock.h"

G_DEFINE_TYPE_WITH_CODE(SCSlideshow, sc_slideshow, GTK_TYPE_WINDOW, NULL)


static void sc_slideshow_init(SCSlideshow *ss)
{
}


void sc_slideshow_class_init(SCSlideshowClass *klass)
{
}


static void redraw(SCSlideshow *ss)
{
    gint w, h;
    w = gtk_widget_get_allocated_width(GTK_WIDGET(ss->drawingarea));
    h = gtk_widget_get_allocated_height(GTK_WIDGET(ss->drawingarea));
    gtk_widget_queue_draw_area(ss->drawingarea, 0, 0, w, h);
}


static gint ssh_destroy_sig(GtkWidget *widget, SCSlideshow *ss)
{
    if ( ss->blank_cursor != NULL ) {
        g_object_unref(ss->blank_cursor);
    }
    if ( ss->inhibit_cookie ) {
        gtk_application_uninhibit(ss->app, ss->inhibit_cookie);
    }
    return FALSE;
}


static gboolean ss_draw_sig(GtkWidget *da, cairo_t *cr, SCSlideshow *ss)
{
    double dw, dh;  /* Size of drawing area */
    double lw, lh;  /* Logical size of slide */
    double sw, sh;  /* Size of slide on screen */
    double xoff, yoff;

    dw = gtk_widget_get_allocated_width(GTK_WIDGET(da));
    dh = gtk_widget_get_allocated_height(GTK_WIDGET(da));

    /* Overall background */
    cairo_rectangle(cr, 0.0, 0.0, dw, dh);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_fill(cr);

    slide_get_logical_size(ss->cur_slide,
                           narrative_get_stylesheet(ss->n), &lw, &lh);

    if ( lw/lh > (double)dw/dh ) {
        /* Slide is too wide.  Letterboxing top/bottom */
        sw = dw;
        sh = dw * lh/lw;
    } else {
        /* Letterboxing at sides */
        sw = dh * lw/lh;
        sh = dh;
    }

    xoff = (dw - sw)/2.0;
    yoff = (dh - sh)/2.0;

    if ( !ss->blank ) {

        PangoContext *pc;
        int n;
        struct slide_pos sel;

        cairo_save(cr);
        cairo_translate(cr, xoff, yoff);
        cairo_scale(cr, sw/lw, sh/lh);

        sel.para = 0;  sel.pos = 0;  sel.trail = 0;
        n = narrative_get_slide_number_for_slide(ss->n, ss->cur_slide);
        pc = pango_cairo_create_context(cr);

        slide_render_cairo(ss->cur_slide, cr,
                           narrative_get_imagestore(ss->n),
                           narrative_get_stylesheet(ss->n),
                           n, pango_language_get_default(), pc,
                           NULL, sel, sel);

        g_object_unref(pc);
        cairo_restore(cr);

    }

    return FALSE;
}


static gboolean ss_realize_sig(GtkWidget *w, SCSlideshow *ss)
{
    if ( (ss->app == NULL) || colloquium_get_hidepointer(COLLOQUIUM(ss->app)) ) {

        /* Hide the pointer */
        GdkWindow *win;
        win = gtk_widget_get_window(w);
        ss->blank_cursor = gdk_cursor_new_for_display(gdk_display_get_default(),
                                                      GDK_BLANK_CURSOR);
        gdk_window_set_cursor(GDK_WINDOW(win), ss->blank_cursor);

    } else {
        ss->blank_cursor = NULL;
    }

    return FALSE;
}


void sc_slideshow_set_slide(SCSlideshow *ss, Slide *ns)
{
    ss->cur_slide = ns;
    redraw(ss);
}


SCSlideshow *sc_slideshow_new(Narrative *n, GtkApplication *app)
{
    GdkDisplay *display;
    int n_monitors;
    SCSlideshow *ss;

    ss = g_object_new(SC_TYPE_SLIDESHOW, NULL);
    if ( ss == NULL ) return NULL;

    ss->blank = 0;
    ss->n = n;
    ss->cur_slide = NULL;
    ss->blank_cursor = NULL;
    ss->app = app;

    ss->drawingarea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(ss), ss->drawingarea);

    gtk_widget_set_can_focus(GTK_WIDGET(ss->drawingarea), TRUE);
    gtk_widget_add_events(GTK_WIDGET(ss->drawingarea),
                          GDK_KEY_PRESS_MASK);

    g_signal_connect(G_OBJECT(ss), "destroy",
                     G_CALLBACK(ssh_destroy_sig), ss);
    g_signal_connect(G_OBJECT(ss), "realize",
                     G_CALLBACK(ss_realize_sig), ss);
    g_signal_connect(G_OBJECT(ss->drawingarea), "draw",
             G_CALLBACK(ss_draw_sig), ss);

    gtk_widget_grab_focus(GTK_WIDGET(ss->drawingarea));

    display = gdk_display_get_default();
    n_monitors = gdk_display_get_n_monitors(display);

    GdkMonitor *mon_ss;
    if ( n_monitors == 1 ) {
        mon_ss = gdk_display_get_primary_monitor(display);
        printf(_("Single monitor mode\n"));
        ss->single_monitor = 1;
    } else {
        mon_ss = gdk_display_get_monitor(display, 1);
        printf(_("Dual monitor mode\n"));
        ss->single_monitor = 0;
    }

    /* Workaround because gtk_window_fullscreen_on_monitor doesn't work */
    GdkRectangle rect;
    gdk_monitor_get_geometry(mon_ss, &rect);
    gtk_window_move(GTK_WINDOW(ss), rect.x, rect.y);
    gtk_window_fullscreen(GTK_WINDOW(ss));

    if ( app != NULL ) {
        ss->inhibit_cookie = gtk_application_inhibit(app, GTK_WINDOW(ss),
                                                     GTK_APPLICATION_INHIBIT_IDLE,
                                                     _("Presentation slide show is running"));
    }

    return ss;
}

