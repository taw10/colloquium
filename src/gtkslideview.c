/*
 * gtkslideview.c
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


G_DEFINE_TYPE_WITH_CODE(GtkSlideView, gtk_slide_view, GTK_TYPE_DRAWING_AREA,
                        NULL)


static void gtk_slide_view_class_init(GtkSlideViewClass *klass)
{
}


static void gtk_slide_view_init(GtkSlideView *e)
{
}


static void gtksv_redraw(GtkSlideView *e)
{
    gtk_widget_queue_draw(GTK_WIDGET(e));
}


static gint gtksv_destroy_sig(GtkWidget *window, GtkSlideView *e)
{
    return 0;
}


static void gtksv_draw_sig(GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer vp)
{
    GtkSlideView *e = vp;
    float aspect;
    float aw;
    float bx, by;

    aspect = slide_get_aspect(e->slide);
    letterbox(w, h, aspect, &aw, &bx, &by);

    /* Ultimate background */
    if ( e->bg_pixbuf != NULL ) {
        gdk_cairo_set_source_pixbuf(cr, e->bg_pixbuf, 0.0, 0.0);
        cairo_pattern_t *patt = cairo_get_source(cr);
        cairo_pattern_set_extend(patt, CAIRO_EXTEND_REPEAT);
        cairo_paint(cr);
    } else {
        cairo_set_source_rgba(cr, 0.8, 0.8, 1.0, 1.0);
        cairo_paint(cr);
    }

    cairo_translate(cr, bx, by);
    cairo_save(cr);
    slide_render_cairo(e->slide, cr, aw);
    cairo_restore(cr);

    if ( e->show_laser ) {
        cairo_arc(cr, aw*e->laser_x, (aw/aspect)*e->laser_y, 10.0, 0, 2*M_PI);
        cairo_set_source_rgba(cr, 0.2, 1.0, 0.1, 0.8);
        cairo_fill(cr);
    }
}


void gtk_slide_view_set_slide(GtkWidget *widget, Slide *slide)
{
    GtkSlideView *e = GTK_SLIDE_VIEW(widget);
    e->slide = slide;
    gtksv_redraw(e);
}


GtkWidget *gtk_slide_view_new(Narrative *n, Slide *slide)
{
    GtkSlideView *sv;
    GError *err;

    sv = g_object_new(GTK_TYPE_SLIDE_VIEW, NULL);

    sv->n = n;
    sv->slide = slide;
    sv->show_laser = 0;

    err = NULL;
    sv->bg_pixbuf = gdk_pixbuf_new_from_resource("/uk/me/bitwiz/colloquium/sky.png",
                                                       &err);
    if ( sv->bg_pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load background: %s\n"), err->message);
    }

    gtk_widget_set_size_request(GTK_WIDGET(sv), 100, 100);
    g_signal_connect(G_OBJECT(sv), "destroy", G_CALLBACK(gtksv_destroy_sig), sv);
    gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(sv),
                                   gtksv_draw_sig, sv, NULL);
    gtk_widget_grab_focus(GTK_WIDGET(sv));
    return GTK_WIDGET(sv);
}


void gtk_slide_view_set_laser(GtkSlideView *sv, double x, double y)
{
    sv->show_laser = 1;
    sv->laser_x = x;
    sv->laser_y = y;
    gtk_widget_queue_draw(GTK_WIDGET(sv));
}


void gtk_slide_view_set_laser_off(GtkSlideView *sv)
{
    sv->show_laser = 0;
    gtk_widget_queue_draw(GTK_WIDGET(sv));
}


void gtk_slide_view_widget_to_relative_coords(GtkSlideView *sv, gdouble *px, gdouble *py)
{
    float aspect;
    float aw;
    float bx, by;
    double x, y;
    int w, h;

    w = gtk_widget_get_width(GTK_WIDGET(sv));
    h = gtk_widget_get_height(GTK_WIDGET(sv));
    aspect = slide_get_aspect(sv->slide);
    letterbox(w, h, aspect, &aw, &bx, &by);

    x = *px;   y = *py;
    *px = (x-bx)/aw;
    *py = (y-by)/(aw/aspect);
}
