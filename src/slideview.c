/*
 * slideview.c
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


G_DEFINE_FINAL_TYPE(SlideView, colloquium_slide_view, GTK_TYPE_WIDGET)

static void slide_view_finalize(GObject *object);
static void slide_view_realize(GtkWidget *w);
static void slide_view_size_allocate(GtkWidget *widget, int w, int h, int baseline);
static void slide_view_dispose(GObject *object);

static void colloquium_slide_view_class_init(SlideViewClass *klass)
{
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    wklass->realize = slide_view_realize;
    wklass->size_allocate = slide_view_size_allocate;
    oklass->finalize = slide_view_finalize;
    oklass->dispose = slide_view_dispose;
}


static void colloquium_slide_view_init(SlideView *e)
{
}


static void gtksv_redraw(SlideView *e)
{
    e->widget_w_for_texture = 0;
    gtk_widget_queue_draw(GTK_WIDGET(e));
}


static void slide_view_finalize(GObject *object)
{
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(object);
    g_source_remove(sv->laser_timeout_source_id);
    G_OBJECT_CLASS(colloquium_slide_view_parent_class)->finalize(object);
}


static void slide_view_dispose(GObject *object)
{
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(object);
    gtk_widget_unparent(sv->offload);
}


static void slide_view_realize(GtkWidget *w)
{
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(w);

    GTK_WIDGET_CLASS(colloquium_slide_view_parent_class)->realize(w);
}


#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define MEMFORMAT GDK_MEMORY_B8G8R8X8
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define MEMFORMAT GDK_MEMORY_X8R8G8B8
#else
#define MEMFORMAT "unknown byte order"
#endif

static void update_sv_texture(SlideView *sv, int w)
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

#if 0
    if ( sv->show_laser ) {
        int x = aw*sv->laser_x - 11;
        int y = (aw/aspect)*sv->laser_y - 11;
        cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(x,y,22,22));
        cairo_arc(cr, aw*sv->laser_x, (aw/aspect)*sv->laser_y, 10.0, 0, 2*M_PI);
        cairo_set_source_rgba(cr, 0.2, 1.0, 0.1, 0.8);
        cairo_fill(cr);
        cairo_destroy(cr);
    }
#endif


void slide_view_set_slide(GtkWidget *widget, Slide *slide)
{
    SlideView *e = COLLOQUIUM_SLIDE_VIEW(widget);
    e->slide = slide;
    gtksv_redraw(e);
}


static gboolean laser_timeout(gpointer vp)
{
    SlideView *sv = vp;
    if ( !sv->show_laser ) return G_SOURCE_CONTINUE;
    if ( g_get_monotonic_time() > 500000+sv->last_laser ) {
        slide_view_set_laser_off(sv);
    }
    return G_SOURCE_CONTINUE;
}


static void slide_view_size_allocate(GtkWidget *widget, int w, int h, int baseline)
{
    GtkAllocation alloc;
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(widget);

    alloc.x = 0;
    alloc.y = 0;
    alloc.width = w;
    alloc.height = h;
    gtk_widget_size_allocate(sv->offload, &alloc, -1);

    if ( (sv->texture == NULL) || (sv->widget_w_for_texture != w) ) {
        update_sv_texture(sv, w);
        gtk_picture_set_paintable(GTK_PICTURE(sv->image), GDK_PAINTABLE(sv->texture));
    }
}


GtkWidget *slide_view_new(Narrative *n, Slide *slide)
{
    SlideView *sv;

    sv = g_object_new(COLLOQUIUM_TYPE_SLIDE_VIEW, NULL);

    sv->n = n;
    sv->slide = slide;
    sv->show_laser = 0;
    sv->widget_w_for_texture = 0;
    sv->texture = NULL;

    sv->image = gtk_picture_new();
    sv->offload = gtk_graphics_offload_new(sv->image);
    gtk_widget_set_parent(GTK_WIDGET(sv->offload), GTK_WIDGET(sv));
    gtk_graphics_offload_set_black_background(GTK_GRAPHICS_OFFLOAD(sv->offload), TRUE);

    gtk_widget_set_size_request(GTK_WIDGET(sv), 100, 100);
    gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(sv));
    sv->laser_timeout_source_id = g_timeout_add_seconds(1, laser_timeout, sv);
    return GTK_WIDGET(sv);
}


void slide_view_set_laser(SlideView *sv, double x, double y)
{
    sv->show_laser = 1;
    sv->laser_x = x;
    sv->laser_y = y;
    sv->last_laser = g_get_monotonic_time();
    gtk_widget_queue_draw(GTK_WIDGET(sv));
}


void slide_view_set_laser_off(SlideView *sv)
{
    sv->show_laser = 0;
    gtk_widget_queue_draw(GTK_WIDGET(sv));
}


void slide_view_widget_to_relative_coords(SlideView *sv, gdouble *px, gdouble *py)
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
