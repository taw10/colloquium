/*
 * laseroverlay.h
 *
 * Copyright © 2025 Thomas White <taw@bitwiz.me.uk>
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

#ifndef COLLOQUIUM_LASER_OVERLAY_H
#define COLLOQUIUM_LASER_OVERLAY_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

typedef struct _colloquiumlaseroverlay LaserOverlay;
typedef struct _colloquiumlaseroverlayclass LaserOverlayClass;

#define COLLOQUIUM_TYPE_LASER_OVERLAY (colloquium_laser_overlay_get_type())

#define COLLOQUIUM_LASER_OVERLAY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                       COLLOQUIUM_TYPE_LASER_OVERLAY, LaserOverlay))

struct _colloquiumlaseroverlay
{
    GtkWidget            parent_instance;

    /*< private >*/
    int                  show_laser;
    double               laser_x;
    double               laser_y;
    gint64               last_laser;
    guint                laser_timeout_source_id;
    double               offs_x;
    double               offs_y;
    double               image_w;
    double               image_h;
};

struct _colloquiumlaseroverlayclass
{
    GtkWidgetClass parent_class;
};

extern GType colloquium_laser_overlay_get_type(void);

extern GtkWidget *laser_overlay_new(void);
extern void laser_overlay_set_laser(LaserOverlay *lo, double x, double y);
extern void laser_overlay_set_laser_off(LaserOverlay *lo);
extern void laser_overlay_set_letterbox(LaserOverlay *lo, double x, double y, double w, double h);

#endif  /* COLLOQUIUM_LASER_OVERLAY_H */
