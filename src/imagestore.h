/*
 * imagestore.h
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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

#ifndef IMAGESTORE_H
#define IMAGESTORE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <gio/gio.h>

typedef struct _imagestore ImageStore;

extern ImageStore *imagestore_new(const char *storename);
extern void imagestore_destroy(ImageStore *is);
extern void imagestore_set_parent(ImageStore *is, GFile *parent);
extern void show_imagestore(ImageStore *is);

extern cairo_surface_t *lookup_image(ImageStore *is, const char *uri, int w);
extern int imagestore_get_size(ImageStore *is, const char *uri, int *w, int *h);

#endif	/* IMAGESTORE_H */
