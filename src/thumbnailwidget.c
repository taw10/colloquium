/*
 * thumbnailwidget.c
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

#include "thumbnailwidget.h"
#include "narrative_window.h"
#include "slide_render_cairo.h"
#include "slide.h"
#include "slide_window.h"


G_DEFINE_FINAL_TYPE(Thumbnail, colloquium_thumbnail, GTK_TYPE_WIDGET)


static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot);

static void colloquium_thumbnail_class_init(ThumbnailClass *klass)
{
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->snapshot = thumbnail_snapshot;
}


static void colloquium_thumbnail_init(Thumbnail *e)
{
}


static void click_sig(GtkGestureClick *self, int n_press, gdouble x, gdouble y, gpointer vp)
{
    Thumbnail *th = vp;

    if ( n_press != 2 ) return;

    if ( th->nw->show == NULL ) {
        if ( th->nw->n_slidewindows < 16 ) {
            SlideWindow *sw = slide_window_new(th->nw->n, th->slide, th->nw, th->nw->app);
            th->nw->slidewindows[th->nw->n_slidewindows++] = sw;
            g_signal_connect(G_OBJECT(sw), "destroy",
                             G_CALLBACK(slide_window_closed_sig), th->nw);
            gtk_window_present(GTK_WINDOW(sw));
        } else {
            fprintf(stderr, _("Too many slide windows\n"));
        }
    } else {
        sc_slideshow_set_slide(th->nw->show, th->slide);
    }
}


#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define MEMFORMAT GDK_MEMORY_B8G8R8X8
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define MEMFORMAT GDK_MEMORY_X8R8G8B8
#else
#define MEMFORMAT "unknown byte order"
#endif

static void update_thumbnail_texture(Thumbnail *th, double view_scale, int w, int h)
{
    cairo_t *cr;
    cairo_surface_t *surf;
    GBytes *bytes;

    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surf);
    cairo_scale(cr, view_scale, view_scale);
    slide_render_cairo(th->slide, cr);
    cairo_destroy(cr);

    bytes = g_bytes_new_with_free_func(cairo_image_surface_get_data(surf),
                                       h*cairo_image_surface_get_stride(surf),
                                       (GDestroyNotify)cairo_surface_destroy,
                                       cairo_surface_reference(surf));

    th->texture = gdk_memory_texture_new(w, h, MEMFORMAT, bytes,
                                         cairo_image_surface_get_stride(surf));

    g_bytes_unref(bytes);
    th->texture_scale = view_scale;
}


static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot)
{
    double logical_w, logical_h;
    Thumbnail *th = COLLOQUIUM_THUMBNAIL(da);
    int w, h;
    graphene_rect_t rect;
    double view_scale, aw, ah, sx, sy;
    double border_offs_x, border_offs_y;

    slide_get_logical_size(th->slide, &logical_w, &logical_h);
    w = gtk_widget_get_width(da);
    h = gtk_widget_get_height(da);
    sx = (double)w / logical_w;
    sy = (double)h / logical_h;
    view_scale = (sx < sy) ? sx : sy;
    aw = view_scale * logical_w;
    ah = view_scale * logical_h;
    border_offs_x = (w - aw)/2.0;
    border_offs_y = (h - ah)/2.0;

    if ( (th->texture == NULL) || (th->texture_scale != view_scale) ) {
        update_thumbnail_texture(th, view_scale, aw, ah);
    }

    GskRoundedRect rrect;
    rect = GRAPHENE_RECT_INIT(border_offs_x, border_offs_y, aw, ah);
    gsk_rounded_rect_init_from_rect(&rrect, &rect, 3);
    gtk_snapshot_push_rounded_clip(snapshot, &rrect);
    gtk_snapshot_translate(snapshot, &GRAPHENE_POINT_INIT(border_offs_x, border_offs_y));
    gtk_snapshot_append_texture(snapshot, th->texture, &GRAPHENE_RECT_INIT(0,0,aw,ah));
    gtk_snapshot_pop(snapshot);

    GdkRGBA color = { 0.1f, 0.1f, 0.1f, 0.8f };
    GdkRGBA colors[] = { color, color, color, color };
    float widths[4] = {1.0f,1.0f,1.0f,1.0f};
    gtk_snapshot_append_border(snapshot, &rrect, widths, colors);
}


static GdkContentProvider *drag_prepare(GtkDragSource *ds, double x, double y, Thumbnail *th)
{
    GValue val;

    g_value_init(&val, COLLOQUIUM_TYPE_THUMBNAIL);
    g_value_set_object(&val, G_OBJECT(th));
    return  gdk_content_provider_new_for_value(&val);
}


GtkWidget *thumbnail_new(Slide *slide, NarrativeWindow *nw)
{
    Thumbnail *th;
    double w, h;
    GtkGesture *evc;

    th = g_object_new(COLLOQUIUM_TYPE_THUMBNAIL, NULL);
    th->nw = nw;
    th->slide = slide;

    slide_get_logical_size(th->slide, &w, &h);
    gtk_widget_set_size_request(GTK_WIDGET(th), 320*w/h, 320);
    th->texture = NULL;

    th->cursor = gdk_cursor_new_from_name("pointer", NULL);
    gtk_widget_set_cursor(GTK_WIDGET(th), th->cursor);

    evc = gtk_gesture_click_new();
    gtk_widget_add_controller(GTK_WIDGET(th), GTK_EVENT_CONTROLLER(evc));
    g_signal_connect(G_OBJECT(evc), "pressed", G_CALLBACK(click_sig), th);

    th->drag_source = gtk_drag_source_new();
    gtk_widget_add_controller(GTK_WIDGET(th), GTK_EVENT_CONTROLLER(th->drag_source));
    g_signal_connect(G_OBJECT(th->drag_source), "prepare",
                     G_CALLBACK(drag_prepare), th);

    return GTK_WIDGET(th);
}


Slide *thumbnail_get_slide(Thumbnail *th)
{
    return th->slide;
}
