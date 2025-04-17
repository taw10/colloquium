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
    double sx, sy;
    double aw, ah;
    double log_w, log_h;

    if ( slide_get_logical_size(e->slide, &log_w, &log_h) ) {
        fprintf(stderr, "Failed to get logical size\n");
        return;
    }

    e->w = w;
    e->h = h;
    sx = (double)e->w / log_w;
    sy = (double)e->h / log_h;
    e->view_scale = (sx < sy) ? sx : sy;

    /* Actual size (in device units) */
    aw = e->view_scale * log_w;
    ah = e->view_scale * log_h;

    e->border_offs_x = (w - aw)/2.0;
    e->border_offs_y = (h - ah)/2.0;

    e->visible_height = h;
    e->visible_width = w;

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

    cairo_translate(cr, e->border_offs_x, e->border_offs_y);
    cairo_translate(cr, -e->h_scroll_pos, -e->v_scroll_pos);
    cairo_scale(cr, e->view_scale, e->view_scale);

    slide_render_cairo(e->slide, cr);
}

void gtk_slide_view_set_scale(GtkSlideView *e, double scale)
{
    e->view_scale = 1.0;
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
    sv->w = 100;
    sv->h = 100;
    sv->border_offs_x = 0;
    sv->border_offs_y = 0;
    sv->min_border = 0.0;
    sv->h_scroll_pos = 0;
    sv->v_scroll_pos = 0;
    sv->view_scale = 1.0;

    err = NULL;
    sv->bg_pixbuf = gdk_pixbuf_new_from_resource("/uk/me/bitwiz/colloquium/sky.png",
                                                       &err);
    if ( sv->bg_pixbuf == NULL ) {
        fprintf(stderr, _("Failed to load background: %s\n"), err->message);
    }

    gtk_widget_set_size_request(GTK_WIDGET(sv), sv->w, sv->h);
    g_signal_connect(G_OBJECT(sv), "destroy", G_CALLBACK(gtksv_destroy_sig), sv);
    gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(sv),
                                   gtksv_draw_sig, sv, NULL);
    gtk_widget_grab_focus(GTK_WIDGET(sv));
    return GTK_WIDGET(sv);
}
