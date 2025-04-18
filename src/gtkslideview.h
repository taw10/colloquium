/*
 * sc_editor.h
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

#ifndef GTK_SLIDE_VIEW_H
#define GTK_SLIDE_VIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>


#include "narrative.h"
#include "slide.h"


#define GTK_TYPE_SLIDE_VIEW (gtk_slide_view_get_type())

#define GTK_SLIDE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                 GTK_TYPE_SLIDE_VIEW, GtkSlideView))

#define GTK_IS_SLIDE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                    GTK_TYPE_SLIDE_VIEW))

#define GTK_SLIDE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((obj), \
                                         GTK_TYPE_SLIDE_VIEW, GtkSlideViewClass))

#define GTK_IS_SLIDE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                            GTK_TYPE_SLIDE_VIEW))

#define GTK_SLIDE_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                           GTK_TYPE_SLIDE_VIEW, GtkSlideViewClass))


struct _gtkslideview
{
    GtkDrawingArea       parent_instance;

    /*< private >*/
    Narrative           *n;
    Slide               *slide;
    GdkPixbuf           *bg_pixbuf;
};

struct _gtkslideviewclass
{
    GtkDrawingAreaClass parent_class;
};

typedef struct _gtkslideview GtkSlideView;
typedef struct _gtkslideviewclass GtkSlideViewClass;

extern GType gtk_slide_view_get_type(void);
extern GtkWidget *gtk_slide_view_new(Narrative *n, Slide *slide);
extern void gtk_slide_view_set_slide(GtkWidget *sv, Slide *slide);

#endif  /* GTK_SLIDE_VIEW_H */
