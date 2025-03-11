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

#include "thumbnailwidget.h"
#include "slide_render_cairo.h"
#include "slide.h"


G_DEFINE_TYPE_WITH_CODE(GtkThumbnail, gtk_thumbnail, GTK_TYPE_DRAWING_AREA, NULL)


static void gtk_thumbnail_class_init(GtkThumbnailClass *klass)
{
}


static void gtk_thumbnail_init(GtkThumbnail *e)
{
}


static void thumbnail_draw_sig(GtkDrawingArea *da, cairo_t *cr,
                               int w, int h, gpointer vp)
{
    double logical_w, logical_h;
    GtkThumbnail *th = GTK_THUMBNAIL(da);
    slide_get_logical_size(th->slide, &logical_w, &logical_h);
    cairo_scale(cr, (double)w/logical_w, (double)h/logical_h);
    slide_render_cairo(th->slide, cr);

    cairo_rectangle(cr, 0, 0, logical_w, logical_h);
    cairo_set_line_width(cr, 2);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_stroke(cr);
}


GtkWidget *gtk_thumbnail_new(Slide *slide)
{
    GtkThumbnail *th;
    double w, h;

    th = g_object_new(GTK_TYPE_THUMBNAIL, NULL);
    th->slide = slide;

    slide_get_logical_size(th->slide, &w, &h);
    gtk_widget_set_size_request(GTK_WIDGET(th), 320*w/h, 320);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(th),
                                   thumbnail_draw_sig, th, NULL);
    return GTK_WIDGET(th);
}
