/*
 * imagestore.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libgen.h>
#include <cairo.h>
#include <glib.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "imagestore.h"

#define MAX_SIZES (32)

struct image_record
{
	char *filename;
	cairo_surface_t *surf[MAX_SIZES];
	int w[MAX_SIZES];
	int last_used[MAX_SIZES];
	int n_sizes;
	int h_last_used;
};


struct _imagestore
{
	int n_images;
	struct image_record *images;
	int max_images;
	GFile *pparent;
	GFile *iparent;
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
	is->pparent = NULL;
	if ( storename != NULL ) {
		is->iparent = g_file_new_for_uri(storename);
	} else {
		is->iparent = NULL;
	}
	is->n_images = 0;
	is->max_images = 0;
	if ( alloc_images(is, 32) ) {
		free(is);
		return NULL;
	}

	return is;
}


void imagestore_set_parent(ImageStore *is, GFile *parent)
{
	if ( is->pparent != NULL ) {
		g_object_unref(is->pparent);
	}
	is->pparent = parent;
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
	g_object_unref(is->pparent);
	g_object_unref(is->iparent);
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


static int try_load(GFile *file, GdkPixbuf **pixbuf, gint w, gint h)
{
	GFileInputStream *stream;
	GError *error = NULL;

	stream = g_file_read(file, NULL, &error);
	if ( stream != NULL ) {
		GError *pberr = NULL;
		*pixbuf = gdk_pixbuf_new_from_stream_at_scale(G_INPUT_STREAM(stream),
		                                              w, h, TRUE,
		                                              NULL, &pberr);
		g_object_unref(stream);
		g_object_unref(file);
		return 1;
	}

	return 0;
}


static GdkPixbuf *load_image(const char *uri, GFile *pparent, GFile *iparent,
                             gint w, gint h)
{
	GFile *file;
	GdkPixbuf *pixbuf;

	/* Literal pathname */
	file = g_file_new_for_path(uri);
	if ( try_load(file, &pixbuf, w, h) ) return pixbuf;

	/* Try the file prefixed with the directory the presentation is in */
	if ( pparent != NULL ) {
		file = g_file_get_child(pparent, uri);
		if ( try_load(file, &pixbuf, w, h) ) return pixbuf;
	}

	/* Try prefixing with imagestore folder */
	if ( iparent != NULL ) {
		file = g_file_get_child(iparent, uri);
		if ( try_load(file, &pixbuf, w, h) ) return pixbuf;
	}

	return NULL;
}


int imagestore_get_size(ImageStore *is, const char *filename,
                        int *w, int *h)
{
	GdkPixbuf *pixbuf;

	pixbuf = load_image(filename, is->pparent, is->iparent, -1, -1);
	if ( pixbuf == NULL ) return 1;

	*w = gdk_pixbuf_get_width(pixbuf);
	*h = gdk_pixbuf_get_height(pixbuf);

	return 0;
}


static int find_earliest(struct image_record *im)
{
	int i, earliest, l;

	earliest = im->h_last_used;
	l = im->n_sizes;

	for ( i=0; i<im->n_sizes; i++ ) {
		if ( im->last_used[i] < earliest ) {
			earliest = im->last_used[i];
			l = i;
		}
	}

	assert(l != im->n_sizes);

	cairo_surface_destroy(im->surf[l]);

	return l;
}


static cairo_surface_t *add_image_size(struct image_record *im,
                                       const char *filename,
                                       GFile *pparent, GFile *iparent,
                                       int w)
{
	cairo_surface_t *surf;
	GdkPixbuf *t;
	int pos;

	t = load_image(filename, pparent, iparent, w, -1);
	if ( t == NULL ) return NULL;
	surf = pixbuf_to_surface(t);
	g_object_unref(t);

	/* Add surface to list */
	if ( im->n_sizes == MAX_SIZES ) {
		pos = find_earliest(im);
	} else {
		pos = im->n_sizes++;
	}

	im->w[pos] = w;
	im->surf[pos] = surf;
	im->last_used[pos] = im->h_last_used++;

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
	is->images[idx].h_last_used = 0;

	return add_image_size(&is->images[idx], filename, is->pparent,
	                      is->iparent, w);
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
		if ( is->images[i].w[j] == w ) {
			is->images[i].last_used[j] = is->images[i].h_last_used++;
			return is->images[i].surf[j];
		}
	}

	/* We don't have this size yet */
	surf = add_image_size(&is->images[i], filename, is->pparent,
	                      is->iparent, w);
	return surf;
}
