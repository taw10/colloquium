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


#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define MEMFORMAT GDK_MEMORY_B8G8R8X8
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define MEMFORMAT GDK_MEMORY_X8R8G8B8
#else
#define MEMFORMAT "unknown byte order"
#endif

static void update_thumbnail_texture(Thumbnail *th, int w, int h)
{
    cairo_t *cr;
    cairo_surface_t *surf;
    GBytes *bytes;

    if ( th->texture != NULL ) {
        g_object_unref(th->texture);
        th->texture = NULL;
    }

    surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cr = cairo_create(surf);
    slide_render_cairo(th->slide, cr, w);
    cairo_destroy(cr);

    bytes = g_bytes_new_with_free_func(cairo_image_surface_get_data(surf),
                                       h*cairo_image_surface_get_stride(surf),
                                       (GDestroyNotify)cairo_surface_destroy,
                                       cairo_surface_reference(surf));

    th->texture = gdk_memory_texture_new(w, h, MEMFORMAT, bytes,
                                         cairo_image_surface_get_stride(surf));

    g_bytes_unref(bytes);
    cairo_surface_destroy(surf);
    th->widget_w_for_texture = w;
}


static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot)
{
    Thumbnail *th = COLLOQUIUM_THUMBNAIL(da);
    float aspect;
    int w, h;
    graphene_rect_t rect;
    float aw, ah;
    float border_offs_x, border_offs_y;
    int awi;
    GdkRGBA color;

    aspect = slide_get_aspect(th->slide);
    w = gtk_widget_get_width(da);
    h = gtk_widget_get_height(da);

    letterbox(w, h, aspect, &aw, &border_offs_x, &border_offs_y);
    ah = aw/aspect;

    awi = aw;
    if ( (th->texture == NULL) || (th->widget_w_for_texture != awi) ) {
        update_thumbnail_texture(th, aw, ah);
    }

    GskRoundedRect rrect;
    rect = GRAPHENE_RECT_INIT(border_offs_x, border_offs_y, aw, ah);
    gsk_rounded_rect_init_from_rect(&rrect, &rect, 3);
    gtk_snapshot_push_rounded_clip(snapshot, &rrect);
    gtk_snapshot_translate(snapshot, &GRAPHENE_POINT_INIT(border_offs_x, border_offs_y));
    gtk_snapshot_append_texture(snapshot, th->texture, &GRAPHENE_RECT_INIT(0,0,aw,ah));
    gtk_snapshot_pop(snapshot);

    gtk_widget_get_color(da, &color);
    GdkRGBA colors[] = { color, color, color, color };
    float widths[4] = {1.0f,1.0f,1.0f,1.0f};
    gtk_snapshot_append_border(snapshot, &rrect, widths, colors);
}


static GdkContentProvider *drag_prepare(GtkDragSource *ds, double x, double y, Thumbnail *th)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, COLLOQUIUM_TYPE_THUMBNAIL);
    g_value_set_object(&val, G_OBJECT(th));
    return  gdk_content_provider_new_for_value(&val);
}


GtkWidget *thumbnail_new(Slide *slide, NarrativeWindow *nw)
{
    Thumbnail *th;

    th = g_object_new(COLLOQUIUM_TYPE_THUMBNAIL, NULL);
    th->nw = nw;
    th->slide = slide;

    gtk_widget_set_size_request(GTK_WIDGET(th), 320*slide_get_aspect(th->slide), 320);
    th->texture = NULL;

    gtk_widget_add_css_class(GTK_WIDGET(th), "thumbnail");

    th->cursor = gdk_cursor_new_from_name("pointer", NULL);
    gtk_widget_set_cursor(GTK_WIDGET(th), th->cursor);

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


void thumbnail_set_slide_height(Thumbnail *th, int h)
{
    gtk_widget_set_size_request(GTK_WIDGET(th), h*slide_get_aspect(th->slide), h);
}


void thumbnail_set_slide_width(Thumbnail *th, int w)
{
    gtk_widget_set_size_request(GTK_WIDGET(th), w, w/slide_get_aspect(th->slide));
}
