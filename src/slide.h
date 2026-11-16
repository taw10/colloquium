/*
 * slide.h
 *
 * Copyright © 2019-2025 Thomas White <taw@bitwiz.me.uk>
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

#ifndef SLIDE_H
#define SLIDE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include "file_types.h"

typedef struct _slide Slide;
typedef struct _slideclass SlideClass;

#define COLLOQUIUM_SLIDE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                               COLLOQUIUM_TYPE_SLIDE, Slide))

#define COLLOQUIUM_TYPE_SLIDE (colloquium_slide_get_type())

struct _slide
{
    GObject *parent_instance;

    float aspect;  /* Width divided by height */
    GFile *ext_file;
    int ext_slidenumber;
    GtkTextChildAnchor *anchor;
    enum slide_filetype file_type;
    GtkMediaStream *mediastream;
    char **hide_elements;
    GFile *background;
    char *title;
};

struct _slideclass
{
    GObjectClass parent_class;
};

extern Slide *slide_new(void);
extern Slide *slide_copy(const Slide *s);
extern void slide_free(Slide *s);

extern void slide_set_ext_file(Slide *s, GFile *file);
extern void slide_set_ext_number(Slide *s, int num);
extern void slide_set_hidden_elements(Slide *s, char **elements);
extern void slide_set_background_file(Slide *s, GFile *file);
extern void slide_set_title(Slide *s, const char *title);

extern float slide_get_aspect(Slide *s);
extern GdkPaintable *slide_render(Slide *s, int w);
extern void slide_render_cairo(Slide *s, int w, cairo_t *cr);
extern enum slide_filetype slide_ftype(Slide *s);

#endif /* SLIDE_H */
