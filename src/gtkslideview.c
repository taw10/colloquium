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


G_DEFINE_TYPE_WITH_CODE(GtkSlideView, gtk_slide_view, GTK_TYPE_WIDGET, NULL)

static void slide_view_snapshot(GtkWidget *da, GtkSnapshot *snapshot);

static void gtk_slide_view_class_init(GtkSlideViewClass *klass)
{
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->snapshot = slide_view_snapshot;
}


static void gtk_slide_view_init(GtkSlideView *e)
{
}


static void gtksv_redraw(GtkSlideView *e)
{
    e->widget_w_for_texture = 0;
    gtk_widget_queue_draw(GTK_WIDGET(e));
}


static gint gtksv_destroy_sig(GtkWidget *window, GtkSlideView *e)
{
    return 0;
}


#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define MEMFORMAT GDK_MEMORY_B8G8R8X8
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define MEMFORMAT GDK_MEMORY_X8R8G8B8
#else
#define MEMFORMAT "unknown byte order"
#endif

static void update_sv_texture(GtkSlideView *sv, int w)
{
    cairo_t *cr;
    cairo_surface_t *surf;
    GBytes *bytes;
    float aspect = slide_get_aspect(sv->slide);
    int h = w/aspect;

    if ( sv->texture != NULL ) {
        g_object_unref(sv->texture);
        sv->texture = NULL;
    }

    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surf);
    slide_render_cairo(sv->slide, cr, w);
    cairo_destroy(cr);

    bytes = g_bytes_new_with_free_func(cairo_image_surface_get_data(surf),
                                       h*cairo_image_surface_get_stride(surf),
                                       (GDestroyNotify)cairo_surface_destroy,
                                       cairo_surface_reference(surf));

    sv->texture = gdk_memory_texture_new(w, h, MEMFORMAT, bytes,
                                         cairo_image_surface_get_stride(surf));

    g_bytes_unref(bytes);
    cairo_surface_destroy(surf);
    sv->widget_w_for_texture = w;
}


static void slide_view_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    GtkSlideView *sv = GTK_SLIDE_VIEW(widget);
    float aspect;
    float aw;
    float bx, by;
    int w, h;

    w = gtk_widget_get_width(widget);
    h = gtk_widget_get_height(widget);

    aspect = slide_get_aspect(sv->slide);
    letterbox(w, h, aspect, &aw, &bx, &by);

    /* Ultimate background */
    GdkRGBA col = {0.0, 0.0, 0.0, 1.0};
    gtk_snapshot_append_color(snapshot, &col, &GRAPHENE_RECT_INIT(0,0,w,h));

    int awi = aw;
    if ( (sv->texture == NULL) || (sv->widget_w_for_texture != awi) ) {
        update_sv_texture(sv, awi);
    }

    gtk_snapshot_translate(snapshot, &GRAPHENE_POINT_INIT(bx, by));
    gtk_snapshot_append_texture(snapshot, sv->texture, &GRAPHENE_RECT_INIT(0,0,aw,aw/aspect));

    if ( sv->show_laser ) {
        cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(0,0,w,h));
        cairo_arc(cr, aw*sv->laser_x, (aw/aspect)*sv->laser_y, 10.0, 0, 2*M_PI);
        cairo_set_source_rgba(cr, 0.2, 1.0, 0.1, 0.8);
        cairo_fill(cr);
        cairo_destroy(cr);
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

    sv = g_object_new(GTK_TYPE_SLIDE_VIEW, NULL);

    sv->n = n;
    sv->slide = slide;
    sv->show_laser = 0;
    sv->widget_w_for_texture = 0;
    sv->texture = NULL;

    gtk_widget_set_size_request(GTK_WIDGET(sv), 100, 100);
    g_signal_connect(G_OBJECT(sv), "destroy", G_CALLBACK(gtksv_destroy_sig), sv);
    gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
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
