/*
 * imagestore.h
 *
 * Copyright © 2013-2017 Thomas White <taw@bitwiz.org.uk>
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


typedef struct _imagestore ImageStore;

extern ImageStore *imagestore_new(void);

extern void imagestore_destroy(ImageStore *is);

extern void imagestore_set_presentation_file(ImageStore *is,
                                             const char *filename);

extern cairo_surface_t *lookup_image(ImageStore *is, const char *filename, int w);

extern void show_imagestore(ImageStore *is);

#endif	/* IMAGESTORE_H */
