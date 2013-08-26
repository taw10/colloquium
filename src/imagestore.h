/*
 * imagestore.h
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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

#include <gdk-pixbuf/gdk-pixbuf.h>


typedef struct _imagestore ImageStore;

enum is_size {
	ISZ_SLIDESHOW = 0,
	ISZ_EDITOR = 1,
	ISZ_THUMBNAIL = 2,
	NUM_ISZ_SIZES
};

extern ImageStore *imagestore_new(void);

extern void imagestore_destroy(ImageStore *is);

extern void imagestore_set_presentation_file(ImageStore *is,
                                             const char *filename);

extern GdkPixbuf *lookup_image(ImageStore *is, const char *filename, int w,
                               enum is_size isz);

extern void show_imagestore(ImageStore *is);

#endif	/* IMAGESTORE_H */
