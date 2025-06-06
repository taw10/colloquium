/*
 * thumbnailwidget.h
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

#ifndef COLLOQUIUM_THUMBNAIL_H
#define COLLOQUIUM_THUMBNAIL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

typedef struct _colloquiumthumbnail Thumbnail;
typedef struct _colloquiumthumbnailclass ThumbnailClass;

#include "slide.h"
#include "narrative_window.h"

#define COLLOQUIUM_TYPE_THUMBNAIL (colloquium_thumbnail_get_type())

#define COLLOQUIUM_THUMBNAIL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                   COLLOQUIUM_TYPE_THUMBNAIL, Thumbnail))

struct _colloquiumthumbnail
{
    GtkWidget            parent_instance;

    /*< private >*/
    Slide               *slide;
    NarrativeWindow     *nw;
    GdkCursor           *cursor;
    GdkTexture          *texture;
    int                  widget_w_for_texture;
    GtkDragSource       *drag_source;
};

struct _colloquiumthumbnailclass
{
    GtkWidgetClass parent_class;
};

extern GType colloquium_thumbnail_get_type(void);

extern GtkWidget *thumbnail_new(Slide *slide, NarrativeWindow *nw);
extern Slide *thumbnail_get_slide(Thumbnail *th);
extern void thumbnail_set_slide_height(Thumbnail *th, int h);
extern void thumbnail_set_slide_width(Thumbnail *th, int w);

#endif  /* COLLOQUIUM_THUMBNAIL_H */
