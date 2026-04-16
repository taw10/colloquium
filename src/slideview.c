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
#include "laseroverlay.h"


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


static void slide_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(colloquium_slide_view_parent_class)->finalize(object);
}


static void slide_view_dispose(GObject *object)
{
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(object);
    gtk_widget_unparent(sv->overlay);
}


static void slide_view_realize(GtkWidget *w)
{
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(w);

    GTK_WIDGET_CLASS(colloquium_slide_view_parent_class)->realize(w);

    if ( sv->slide->file_type == SLIDE_FTYPE_VIDEO ) {
        GdkSurface *surf;
        surf = gtk_native_get_surface(gtk_widget_get_native(sv->picture));
        gtk_media_stream_realize(GTK_MEDIA_STREAM(slide_render(sv->slide, 100)), surf);
    }

}


void slide_view_set_slide(GtkWidget *widget, Slide *slide)
{
    SlideView *e = COLLOQUIUM_SLIDE_VIEW(widget);
    e->slide = slide;

    /* Slide is actually rendered on size_allocate */
    e->need_render = 1;
    gtk_widget_queue_allocate(widget);
}


static void slide_view_size_allocate(GtkWidget *widget, int w, int h, int baseline)
{
    GtkAllocation alloc;
    int old_w;
    SlideView *sv = COLLOQUIUM_SLIDE_VIEW(widget);

    alloc.x = 0;
    alloc.y = 0;
    alloc.width = w;
    alloc.height = h;
    gtk_widget_size_allocate(sv->overlay, &alloc, -1);

    old_w = gdk_paintable_get_intrinsic_width(gtk_picture_get_paintable(GTK_PICTURE(sv->picture)));
    if ( alloc.width > old_w || sv->need_render ) {
        gtk_picture_set_paintable(GTK_PICTURE(sv->picture),
                                  slide_render(sv->slide, alloc.width));
        sv->need_render = 0;
    }

    float aspect, bx, by, aw;
    aspect = slide_get_aspect(sv->slide);
    letterbox(w, h, aspect, &aw, &bx, &by);
    laser_overlay_set_letterbox(COLLOQUIUM_LASER_OVERLAY(sv->laser), bx, by, aw, aw/aspect);
}


GtkWidget *slide_view_new(Narrative *n, Slide *slide)
{
    SlideView *sv;

    sv = g_object_new(COLLOQUIUM_TYPE_SLIDE_VIEW, NULL);

    sv->n = n;
    sv->slide = slide;
    sv->need_render = 1;

    gtk_widget_add_css_class(GTK_WIDGET(sv), "slideview");

    /* Slide will be rendered in response to size_allocate */
    sv->picture = gtk_picture_new_for_paintable(placeholder_image());

    sv->overlay  = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(sv->overlay), GTK_WIDGET(sv->picture));

    sv->laser = laser_overlay_new();
    gtk_overlay_add_overlay(GTK_OVERLAY(sv->overlay), sv->laser);

    gtk_widget_set_parent(GTK_WIDGET(sv->overlay), GTK_WIDGET(sv));
    gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(sv));

    return GTK_WIDGET(sv);
}


void slide_view_set_laser(SlideView *sv, double x, double y)
{
    laser_overlay_set_laser(COLLOQUIUM_LASER_OVERLAY(sv->laser), x, y);
}


void slide_view_set_laser_off(SlideView *sv)
{
    laser_overlay_set_laser_off(COLLOQUIUM_LASER_OVERLAY(sv->laser));
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
