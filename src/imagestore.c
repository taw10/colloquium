/*
 * imagestore.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libgen.h>
#include <cairo.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "imagestore.h"

#define MAX_SIZES (32)

struct image_record
{
	char *filename;
	cairo_surface_t *surf[MAX_SIZES];
	int w[MAX_SIZES];
	int n_sizes;
};


struct _imagestore
{
	int n_images;
	struct image_record *images;
	int max_images;
	char *dname;
	const char *storename;
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


ImageStore *imagestore_new(const char *storename)
{
	ImageStore *is;

	is = calloc(1, sizeof(ImageStore));
	if ( is == NULL ) return NULL;

	is->images = NULL;
	is->dname = NULL;
	is->storename = storename;
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
		for ( j=0; j<is->images[i].n_sizes; j++ ) {
			if ( is->images[i].surf[j] != NULL ) {
				g_object_unref(is->images[i].surf[j]);
			}
		}
	}
	free(is->images);
	free(is);
}


static cairo_surface_t *pixbuf_to_surface(GdkPixbuf *t)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int w, h;

	if ( t == NULL ) return NULL;

	w = gdk_pixbuf_get_width(t);
	h = gdk_pixbuf_get_height(t);

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if ( surf == NULL ) return NULL;

	cr = cairo_create(surf);

	gdk_cairo_set_source_pixbuf(cr, t, 0, 0);
	cairo_pattern_t *patt = cairo_get_source(cr);
	cairo_pattern_set_extend(patt, CAIRO_EXTEND_PAD);
	cairo_pattern_set_filter(patt, CAIRO_FILTER_BEST);
	cairo_paint(cr);

	cairo_destroy(cr);

	return surf;
}


static char *try_all_locations(const char *filename,
                               const char *dname, const char *iname)
{
	if ( g_file_test(filename, G_FILE_TEST_EXISTS) ) {
		return strdup(filename);
	}

	/* Try the file prefixed with the directory the presentation is in */
	if ( dname != NULL ) {
		char *tmp;
		tmp = malloc(strlen(filename) + strlen(dname) + 2);
		if ( tmp == NULL ) return NULL;
		strcpy(tmp, dname);
		strcat(tmp, "/");
		strcat(tmp, filename);
		if ( g_file_test(tmp, G_FILE_TEST_EXISTS) ) {
			return tmp;
		}
		free(tmp);
	}

	/* Try prefixing with imagestore folder */
	if ( iname != NULL ) {
		char *tmp;
		tmp = malloc(strlen(filename) + strlen(iname) + 2);
		if ( tmp == NULL ) return NULL;
		strcpy(tmp, iname);
		strcat(tmp, "/");
		strcat(tmp, filename);
		if ( g_file_test(tmp, G_FILE_TEST_EXISTS) ) {
			return tmp;
		}
		free(tmp);
	}

	return NULL;
}


int imagestore_get_size(ImageStore *is, const char *filename,
                        int *w, int *h)
{
	char *fullfn;

	fullfn = try_all_locations(filename, is->dname, is->storename);
	if ( gdk_pixbuf_get_file_info(fullfn, w, h) == NULL ) {
		return 1;
	}
	return 0;
}


static cairo_surface_t *add_image_size(struct image_record *im,
                                       const char *filename,
                                       const char *dname, const char *iname,
                                       int w)
{
	char *fullfn;
	cairo_surface_t *surf;
	GdkPixbuf *t;
	GError *error = NULL;

	if ( im->n_sizes == MAX_SIZES ) {
		/* FIXME: Nice cache replacement algorithm */
		fprintf(stderr, "Too many image sizes!\n");
		return NULL;
	}

	fullfn = try_all_locations(filename, dname, iname);
	if ( fullfn == NULL ) return NULL;

	t = gdk_pixbuf_new_from_file_at_size(fullfn, w, -1, &error);
	free(fullfn);

	surf = pixbuf_to_surface(t);
	g_object_unref(t);

	/* Add surface to list */
	im->w[im->n_sizes] = w;
	im->surf[im->n_sizes] = surf;
	im->n_sizes++;

	return surf;
}


static cairo_surface_t *add_new_image(ImageStore *is, const char *filename, int w)
{
	int idx;

	if ( is->n_images == is->max_images ) {
		if ( alloc_images(is, is->max_images+32) ) {
			fprintf(stderr, "Couldn't allocate memory.\n");
			return NULL;
		}
	}

	idx = is->n_images++;

	is->images[idx].n_sizes = 0;
	is->images[idx].filename = strdup(filename);

	return add_image_size(&is->images[idx], filename, is->dname,
	                      is->storename, w);
}


void show_imagestore(ImageStore *is)
{
	int i;

	printf("Store %p contains %i records.\n", is, is->n_images);

	for ( i=0; i<is->n_images; i++ ) {

		printf("%s :\n", is->images[i].filename);
		printf("\n");

	}
}


cairo_surface_t *lookup_image(ImageStore *is, const char *filename, int w)
{
	int i, j;
	int found = 0;
	cairo_surface_t *surf;

	for ( i=0; i<is->n_images; i++ ) {
		if ( strcmp(is->images[i].filename, filename) == 0 ) {
			found = 1;
			break;
		}
	}
	if ( !found ) {
		return add_new_image(is, filename, w);
	}

	for ( j=0; j<is->images[i].n_sizes; j++ ) {
		if ( is->images[i].w[j] == w ) return is->images[i].surf[j];
	}

	/* We don't have this size yet */
	surf = add_image_size(&is->images[i], filename, is->dname,
	                      is->storename, w);
	return surf;
}
