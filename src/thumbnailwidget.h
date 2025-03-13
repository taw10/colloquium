/*
 * thumbnailwidget.h
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

#ifndef GTK_THUMBNAIL_H
#define GTK_THUMBNAIL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

#include "slide_render_cairo.h"
#include "narrative_window.h"


#define GTK_TYPE_THUMBNAIL (gtk_thumbnail_get_type())

#define GTK_THUMBNAIL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                 GTK_TYPE_THUMBNAIL, GtkThumbnail))

#define GTK_IS_THUMBNAIL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                    GTK_TYPE_THUMBNAIL))

#define GTK_THUMBNAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((obj), \
                                         GTK_TYPE_THUMBNAIL, GtkThumbnailClass))

#define GTK_IS_THUMBNAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                            GTK_TYPE_THUMBNAIL))

#define GTK_THUMBNAIL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                           GTK_TYPE_THUMBNAIL, GtkThumbnailClass))


struct _gtkthumbnail
{
    GtkDrawingArea       parent_instance;

    /*< private >*/
    Slide               *slide;
    NarrativeWindow     *nw;
    GdkCursor           *cursor;
};

struct _gtkthumbnailclass
{
    GtkDrawingAreaClass parent_class;
};

typedef struct _gtkthumbnail GtkThumbnail;
typedef struct _gtkthumbnailclass GtkThumbnailClass;

extern GType gtk_thumbnail_get_type(void);
extern GtkWidget *gtk_thumbnail_new(Slide *slide, NarrativeWindow *nw);

#endif  /* GTK_THUMBNAIL_H */
