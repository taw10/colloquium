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


static void thumbnail_dispose(GObject *obj);
static void thumbnail_size_allocate(GtkWidget *widget, int w, int h, int baseline);
static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot);


static void colloquium_thumbnail_class_init(ThumbnailClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    oklass->dispose = thumbnail_dispose;
    wklass->size_allocate = thumbnail_size_allocate;
    wklass->snapshot = thumbnail_snapshot;
}


static void colloquium_thumbnail_init(Thumbnail *e)
{
}


static void thumbnail_dispose(GObject *obj)
{
    Thumbnail *th = COLLOQUIUM_THUMBNAIL(obj);
    gtk_widget_unparent(th->picture);
    G_OBJECT_CLASS(colloquium_thumbnail_parent_class)->dispose(obj);
}


static void thumbnail_snapshot(GtkWidget *da, GtkSnapshot *snapshot)
{
    Thumbnail *th = COLLOQUIUM_THUMBNAIL(da);
    float aspect;
    int w, h;
    graphene_rect_t rect;
    float aw, ah;
    float border_offs_x, border_offs_y;
    GdkRGBA color;

    aspect = slide_get_aspect(th->slide);
    w = gtk_widget_get_width(da);
    h = gtk_widget_get_height(da);

    letterbox(w, h, aspect, &aw, &border_offs_x, &border_offs_y);
    ah = aw/aspect;

    GskRoundedRect rrect;
    rect = GRAPHENE_RECT_INIT(border_offs_x, border_offs_y, aw, ah);
    gsk_rounded_rect_init_from_rect(&rrect, &rect, 3);
    gtk_snapshot_push_rounded_clip(snapshot, &rrect);
    gtk_widget_snapshot_child(da, th->picture, snapshot);
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
    return gdk_content_provider_new_for_value(&val);
}


static void thumbnail_size_allocate(GtkWidget *widget, int w, int h, int baseline)
{
    GtkAllocation alloc;
    int old_w;
    Thumbnail *th = COLLOQUIUM_THUMBNAIL(widget);

    alloc.x = 0;
    alloc.y = 0;
    alloc.width = w;
    alloc.height = h;
    gtk_widget_size_allocate(th->picture, &alloc, -1);

    old_w = gdk_paintable_get_intrinsic_width(gtk_picture_get_paintable(GTK_PICTURE(th->picture)));
    if ( alloc.width > old_w || th->need_render ) {
        gtk_picture_set_paintable(GTK_PICTURE(th->picture),
                                  slide_render(th->slide, alloc.width));
        th->need_render = 0;
    }
}


GtkWidget *thumbnail_new(Slide *slide, NarrativeWindow *nw)
{
    Thumbnail *th;

    th = g_object_new(COLLOQUIUM_TYPE_THUMBNAIL, NULL);
    th->nw = nw;
    th->slide = slide;
    th->need_render = 1;

    gtk_widget_add_css_class(GTK_WIDGET(th), "thumbnail");

    th->picture = gtk_picture_new();
    gtk_widget_set_parent(th->picture, GTK_WIDGET(th));

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
    gtk_widget_queue_allocate(GTK_WIDGET(th));
    th->need_render = 1;
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
