/*
 * objects.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2011 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "presentation.h"
#include "objects.h"
#include "mainwindow.h"


struct image_store
{
	int n_images;
	struct image **images;
};


static struct image *add_image_to_store(struct image_store *is, char *filename)
{
	struct image **images_new;
	struct image *i_new;
	int idx;
	GError *error = NULL;
	int w, h;

	i_new = calloc(1, sizeof(struct image));
	if ( i_new == NULL ) return NULL;

	images_new = realloc(is->images,
	                     (is->n_images+1)*sizeof(struct image *));
	if ( images_new == NULL ) {
		fprintf(stderr, "Couldn't allocate memory for image.\n");
		return NULL;
	}
	is->images = images_new;
	idx = is->n_images++;

	gdk_pixbuf_get_file_info(filename, &w, &h);

	/* FIXME: If image is huge, load a smaller version */
	i_new->pb = gdk_pixbuf_new_from_file(filename, &error);
	if ( i_new->pb == NULL ) {
		fprintf(stderr, "Failed to load image '%s'\n", filename);
		is->n_images--;
		return NULL;
	}
	i_new->filename = strdup(filename);
	i_new->refcount = 1;
	i_new->width = w;
	i_new->height = h;
	i_new->parent = is;

	is->images[idx] = i_new;

	return i_new;
}


static struct image *find_filename(struct image_store *is, const char *filename)
{
	int i;

	for ( i=0; i<is->n_images; i++ ) {
		if ( strcmp(is->images[i]->filename, filename) == 0 ) {
			return is->images[i];
		}
	}

	return NULL;
}


struct image *get_image(struct image_store *is, char *filename)
{
	struct image *image;

	image = find_filename(is, filename);
	if ( image == NULL ) {
		image = add_image_to_store(is, filename);
	} else {
		image->refcount++;
	}

	return image;
}


void unref_image(struct image *i)
{
	i->refcount--;

	if ( i->refcount == 0 ) {

		struct image_store *is;
		int j;

		g_object_unref(G_OBJECT(i->pb));
		free(i->filename);
		is = i->parent;
		free(i);

		for ( j=0; j<is->n_images; j++ ) {
			if ( is->images[j] == i ) {
				int k;
				for ( k=j+1; k<is->n_images; k++ ) {
					is->images[k-1] = is->images[k];
				}
				break;
			}
		}
		is->n_images--;

	}
}


struct image_store *image_store_new()
{
	struct image_store *is;

	is = calloc(1, sizeof(*is));
	if ( is == NULL ) return NULL;

	is->images = NULL;
	is->n_images = 0;

	return is;
}


void notify_style_update(struct presentation *p, struct style *sty)
{
	int i;
	int changed = 0;

	for ( i=0; i<p->num_slides; i++ ) {

		int j;
		struct slide *s;

		s = p->slides[i];

		for ( j=0; j<p->slides[i]->num_objects; j++ ) {

			if ( s->objects[j]->style != sty ) continue;

			s->objects[j]->update_object(s->objects[j]);
			if ( p->cur_edit_slide == s ) changed = 1;

		}

	}

	/* Trigger redraw etc */
	p->completely_empty = 0;
	if ( changed ) notify_slide_changed(p, p->cur_edit_slide);
}


static void check_references(struct slide *s, struct object *om)
{
	/* FIXME: Should replace with previous useful one, not NULL */
	if ( s->roles[S_ROLE_PDATE_REF] == om ) {
		struct object *o;
		s->roles[S_ROLE_PDATE_REF] = NULL;
		o = s->roles[S_ROLE_PDATE];
		if ( o != NULL ) o->update_object(o);
	}

	if ( s->roles[S_ROLE_PAUTHOR_REF] == om ) {
		struct object *o;
		s->roles[S_ROLE_PAUTHOR_REF] = NULL;
		o = s->roles[S_ROLE_PAUTHOR];
		if ( o != NULL ) o->update_object(o);
	}

	if ( s->roles[S_ROLE_PTITLE_REF] == om ) {
		struct object *o;
		s->roles[S_ROLE_PTITLE_REF] = NULL;
		o = s->roles[S_ROLE_PTITLE];
		if ( o != NULL ) o->update_object(o);
	}

}


void delete_object(struct object *o)
{
	int i;

	if ( o->parent != NULL ) remove_object_from_slide(o->parent, o);
	o->delete_object(o);

	/* If this object was any kind of special (for this slide),
	 * check all the other slides for references */
	if ( (o->parent->roles[S_ROLE_PTITLE_REF] == o)
	  || (o->parent->roles[S_ROLE_PAUTHOR_REF] == o)
	  || (o->parent->roles[S_ROLE_PDATE_REF] == o) )
	{
		for ( i=0; i<o->parent->parent->num_slides; i++ ) {
			check_references(o->parent->parent->slides[i], o);
		}
	}

	for ( i=0; i<NUM_S_ROLES; i++ ) {
		if ( o->parent->roles[i] == o ) {
			o->parent->roles[i] = NULL;
		}
	}

	free(o);
}


void realise_everything(struct presentation *p)
{
	int i;

	/* Realise all the tools */
	p->select_tool->realise(p->select_tool, p->drawingarea, p);
	p->text_tool->realise(p->text_tool, p->drawingarea, p);
	p->image_tool->realise(p->image_tool, p->drawingarea, p);

	for ( i=0; i<p->num_slides; i++ ) {

		int j;
		struct slide *s;

		s = p->slides[i];

		for ( j=0; j<p->slides[i]->num_objects; j++ ) {
			s->objects[j]->update_object(s->objects[j]);
		}

	}
}


enum corner which_corner(double xp, double yp, struct object *o)
{
	double x, y;  /* Relative to object position */

	x = xp - o->x;
	y = yp - o->y;

	if ( x < 0.0 ) return CORNER_NONE;
	if ( y < 0.0 ) return CORNER_NONE;
	if ( x > o->bb_width ) return CORNER_NONE;
	if ( y > o->bb_height ) return CORNER_NONE;

	/* Top left? */
	if ( (x<20.0) && (y<20.0) ) return CORNER_TL;
	if ( (x>o->bb_width-20.0) && (y<20.0) ) return CORNER_TR;
	if ( (x<20.0) && (y>o->bb_height-20.0) ) {
		return CORNER_BL;
	}
	if ( (x>o->bb_width-20.0) && (y>o->bb_height-20.0) ) {
		return CORNER_BR;
	}

	return CORNER_NONE;
}
