/*
 * laseroverlay.c
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

#include "laseroverlay.h"


G_DEFINE_FINAL_TYPE(LaserOverlay, colloquium_laser_overlay, GTK_TYPE_WIDGET)


static void laser_overlay_snapshot(GtkWidget *w, GtkSnapshot *snapshot);
static void laser_overlay_finalize(GObject *object);

static void colloquium_laser_overlay_class_init(LaserOverlayClass *klass)
{
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    wklass->snapshot = laser_overlay_snapshot;
    oklass->finalize = laser_overlay_finalize;
}


static void colloquium_laser_overlay_init(LaserOverlay *lo)
{
}


static void laser_overlay_snapshot(GtkWidget *wi, GtkSnapshot *snapshot)
{
    LaserOverlay *lo = COLLOQUIUM_LASER_OVERLAY(wi);

    if ( lo->show_laser ) {
        RsvgRectangle viewport;
        float x = lo->offs_x + lo->image_w*lo->laser_x;
        float y = lo->offs_y + lo->image_h*lo->laser_y;
        viewport.x = x;
        viewport.y = y;
        viewport.width = lo->ptr_size;
        viewport.height = lo->ptr_size;
        cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT(x,y,lo->ptr_size,lo->ptr_size));
        rsvg_handle_render_document(lo->svg, cr, &viewport, NULL);
        cairo_destroy(cr);
    }
}

void laser_overlay_set_letterbox(LaserOverlay *lo, double x, double y, double w, double h)
{
    lo->offs_x = x;
    lo->offs_y = y;
    lo->image_w = w;
    lo->image_h = h;
}


static void laser_overlay_finalize(GObject *object)
{
    LaserOverlay *lo = COLLOQUIUM_LASER_OVERLAY(object);
    g_source_remove(lo->laser_timeout_source_id);
    G_OBJECT_CLASS(colloquium_laser_overlay_parent_class)->finalize(object);
}


static gboolean laser_timeout(gpointer vp)
{
    LaserOverlay *lo = vp;
    if ( !lo->show_laser ) return G_SOURCE_CONTINUE;
    if ( g_get_monotonic_time() > 500000+lo->last_laser ) {
        laser_overlay_set_laser_off(lo);
    }
    return G_SOURCE_CONTINUE;
}


GtkWidget *laser_overlay_new()
{
    LaserOverlay *lo;
    GError *error;
    RsvgLength width, height;
    RsvgRectangle viewbox;
    gboolean has_viewbox, has_width, has_height;
    GFile *file;

    lo = g_object_new(COLLOQUIUM_TYPE_LASER_OVERLAY, NULL);
    lo->offs_x = 0;
    lo->offs_y = 0;
    lo->image_w = 100;
    lo->image_h = 100;
    lo->show_laser = 0;
    lo->laser_timeout_source_id = g_timeout_add_seconds(1, laser_timeout, lo);

    GSettings *settings = g_settings_new("uk.me.bitwiz.colloquium");
    char *ptr_uri = g_settings_get_string(settings, "laser-pointer");
    lo->ptr_size = g_settings_get_int(settings, "laser-pointer-size");
    g_object_unref(settings);
    file = g_file_new_for_uri(ptr_uri);
    g_free(ptr_uri);

    error = NULL;
    lo->svg = rsvg_handle_new_from_gfile_sync(file, RSVG_HANDLE_FLAGS_NONE, NULL, &error);
    if ( lo->svg == NULL ) {
        fprintf(stderr, _("Failed to read pointer SVG: %s\n"), error->message);
    }

    rsvg_handle_set_dpi(lo->svg, 96);
    rsvg_handle_get_intrinsic_dimensions(lo->svg, &has_width, &width,
                                         &has_height, &height,
                                         &has_viewbox, &viewbox);

    return GTK_WIDGET(lo);
}


void laser_overlay_set_laser(LaserOverlay *lo, double x, double y)
{
    lo->show_laser = 1;
    lo->laser_x = x;
    lo->laser_y = y;
    lo->last_laser = g_get_monotonic_time();
    gtk_widget_queue_draw(GTK_WIDGET(lo));
}


void laser_overlay_set_laser_off(LaserOverlay *lo)
{
    lo->show_laser = 0;
    gtk_widget_queue_draw(GTK_WIDGET(lo));
}
