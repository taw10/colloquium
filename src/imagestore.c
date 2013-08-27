/*
 * imagestore.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libgen.h>

#include "imagestore.h"

struct image_record
{
	char *filename;
	GdkPixbuf *pixbuf[NUM_ISZ_SIZES];
	int w[NUM_ISZ_SIZES];
};


struct _imagestore
{
	int n_images;
	struct image_record *images;
	int max_images;
	char *dname;
};


static int alloc_images(ImageStore *is, int new_max_images)
{
	struct image_record *images_new;

	images_new = realloc(is->images,
	                     sizeof(struct image_record)*new_max_images);
	if ( images_new == NULL ) return 1;

	is->images = images_new;
	is->max_images = new_max_images;
	return 0;
}


ImageStore *imagestore_new()
{
	ImageStore *is;

	is = calloc(1, sizeof(ImageStore));
	if ( is == NULL ) return NULL;

	is->images = NULL;
	is->dname = NULL;
	is->n_images = 0;
	is->max_images = 0;
	if ( alloc_images(is, 32) ) {
		free(is);
		return NULL;
	}

	return is;
}


void imagestore_set_presentation_file(ImageStore *is, const char *filename)
{
	const char *dname;
	char *cpy;

	/* dirname() is yukky */
	cpy = strdup(filename);
	dname = dirname(cpy);
	free(is->dname);
	is->dname = strdup(dname);
	free(cpy);
}


void imagestore_destroy(ImageStore *is)
{
	int i;

	for ( i=0; i<is->n_images; i++ ) {
		int j;
		free(is->images[i].filename);
		for ( j=0; j<NUM_ISZ_SIZES; j++ ) {
			if ( is->images[i].pixbuf[j] != NULL ) {
				g_object_unref(is->images[i].pixbuf[j]);
			}
		}
	}
	free(is->images);
	free(is);
}


static GdkPixbuf *try_all_locations(const char *filename, int w, ImageStore *is)
{
	GError *error = NULL;
	GdkPixbuf *t;
	char *tmp;
	const char *image_folder = "/home/taw/Documents/Slides"; /* FIXME ! */

	/* Try the filename as is */
	t = gdk_pixbuf_new_from_file_at_size(filename, w, -1, &error);
	if ( t != NULL ) return t;

	/* Try the file prefixed with the directory the presentation is in */
	if ( is->dname != NULL ) {

		error = NULL;
		tmp = malloc(strlen(filename) + strlen(is->dname) + 2);
		if ( tmp == NULL ) return NULL;
		strcpy(tmp, is->dname);
		strcat(tmp, "/");
		strcat(tmp, filename);
		t = gdk_pixbuf_new_from_file_at_size(tmp, w, -1, &error);
		free(tmp);
		if ( t != NULL ) return t;

	}

	/* Try prefixed with image pathname */
	error = NULL;
	tmp = malloc(strlen(filename) + strlen(image_folder) + 2);
	if ( tmp == NULL ) return NULL;
	strcpy(tmp, image_folder);
	strcat(tmp, "/");
	strcat(tmp, filename);
	t = gdk_pixbuf_new_from_file_at_size(tmp, w, -1, &error);
	free(tmp);
	if ( t != NULL ) return t;

	return NULL;
}


static GdkPixbuf *add_pixbuf(struct image_record *im, const char *filename,
                             int w, enum is_size isz, ImageStore *is)
{
	im->pixbuf[isz] = try_all_locations(filename, w, is);

	if ( im->pixbuf[isz] == NULL ) return NULL;

	im->w[isz] = w;

	return im->pixbuf[isz];
}


static GdkPixbuf *add_new_image(ImageStore *is, const char *filename, int w,
                                enum is_size isz)
{
	int j;
	int idx;

	if ( is->n_images == is->max_images ) {
		if ( alloc_images(is, is->max_images+32) ) {
			fprintf(stderr, "Couldn't allocate memory.\n");
			return NULL;
		}
	}

	idx = is->n_images++;

	is->images[idx].filename = strdup(filename);
	for ( j=0; j<NUM_ISZ_SIZES; j++ ) {
		is->images[idx].pixbuf[j] = NULL;
		is->images[idx].w[j] = 0;
	}

	return add_pixbuf(&is->images[idx], filename, w, isz, is);
}


void show_imagestore(ImageStore *is)
{
	int i;

	printf("Store %p contains %i records.\n", is, is->n_images);

	for ( i=0; i<is->n_images; i++ ) {

		printf("%s :\n", is->images[i].filename);
		printf("ss: %p %i  ", is->images[i].pixbuf[ISZ_SLIDESHOW],
		                      is->images[i].w[ISZ_SLIDESHOW]);
		printf("ed: %p %i  ", is->images[i].pixbuf[ISZ_EDITOR],
		                      is->images[i].w[ISZ_EDITOR]);
		printf("th: %p %i  ", is->images[i].pixbuf[ISZ_THUMBNAIL],
		                      is->images[i].w[ISZ_THUMBNAIL]);
		printf("\n");

	}
}


GdkPixbuf *lookup_image(ImageStore *is, const char *filename, int w,
                        enum is_size isz)
{
	int i;
	int found = 0;
	GdkPixbuf *pb;

	for ( i=0; i<is->n_images; i++ ) {
		if ( strcmp(is->images[i].filename, filename) == 0 ) {
			found = 1;
			break;
		}
	}
	if ( !found ) {
		return add_new_image(is, filename, w, isz);
	}

	/* Image already exists, but might not have the right size or
	 * the right slot filled in */

	if ( is->images[i].w[isz] == w ) return is->images[i].pixbuf[isz];

	/* Image is the wrong size or slot is not filled in yet */
	if ( is->images[i].pixbuf[isz] != NULL ) {
		g_object_unref(is->images[i].pixbuf[isz]);
		is->images[i].pixbuf[isz] = NULL;
	}

	/* Slot is not filled in yet */
	pb = add_pixbuf(&is->images[i], filename, w, isz, is);
	return pb;
}
