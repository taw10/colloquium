/*
 * slideview.h
 *
 * Copyright © 2014-2019 Thomas White <taw@bitwiz.org.uk>
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

#ifndef SLIDE_VIEW_H
#define SLIDE_VIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>


#include "narrative.h"
#include "slide.h"

typedef struct _colloquiumslideview SlideView;
typedef struct _colloquiumslideviewclass SlideViewClass;

#define COLLOQUIUM_TYPE_SLIDE_VIEW (colloquium_slide_view_get_type())

#define COLLOQUIUM_SLIDE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                    COLLOQUIUM_TYPE_SLIDE_VIEW, SlideView))


struct _colloquiumslideview
{
    GtkDrawingArea       parent_instance;

    /*< private >*/
    Narrative           *n;
    Slide               *slide;
    int                  show_laser;
    double               laser_x;
    double               laser_y;
    GdkTexture          *texture;
    int                  widget_w_for_texture;
    gint64               last_laser;
    guint                laser_timeout_source_id;
};

struct _colloquiumslideviewclass
{
    GtkWidgetClass parent_class;
};

extern GType colloquium_slide_view_get_type(void);

extern GtkWidget *slide_view_new(Narrative *n, Slide *slide);
extern void slide_view_set_slide(GtkWidget *sv, Slide *slide);
extern void slide_view_set_laser(SlideView *sv, double x, double y);
extern void slide_view_set_laser_off(SlideView *sv);
extern void slide_view_widget_to_relative_coords(SlideView *sv, gdouble *px, gdouble *py);

#endif  /* SLIDE_VIEW_H */
