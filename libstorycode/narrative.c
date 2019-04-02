/*
 * narrative.c
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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
#include <stdio.h>
#include <gio/gio.h>

#include <libintl.h>
#define _(x) gettext(x)

#ifdef HAVE_PANGO
#include <pango/pango.h>
#endif

#ifdef HAVE_CAIRO
#include <cairo.h>
#endif

#include "stylesheet.h"
#include "slide.h"
#include "narrative.h"
#include "narrative_priv.h"
#include "imagestore.h"
#include "storycode.h"

Narrative *narrative_new()
{
	Narrative *n;
	n = malloc(sizeof(*n));
	if ( n == NULL ) return NULL;
	n->n_items = 0;
	n->items = NULL;
	n->stylesheet = NULL;
	n->imagestore = imagestore_new("."); /* FIXME: From app config */
	n->saved = 1;
#ifdef HAVE_PANGO
	n->language = pango_language_to_string(pango_language_get_default());
#else
	n->language = NULL;
#endif
	return n;
}


static void narrative_item_destroy(struct narrative_item *item)
{
	free(item->text);
#ifdef HAVE_PANGO
	if ( item->layout != NULL ) {
		g_object_unref(item->layout);
	}
#endif
#ifdef HAVE_CAIRO
	if ( item->slide_thumbnail != NULL ) {
		cairo_surface_destroy(item->slide_thumbnail);
	}
#endif
}


void narrative_free(Narrative *n)
{
	int i;
	for ( i=0; i<n->n_items; i++ ) {
		narrative_item_destroy(&n->items[i]);
	}
	free(n->items);
	free(n);
}


Narrative *narrative_load(GFile *file)
{
	GBytes *bytes;
	const char *text;
	size_t len;
	Narrative *n;

	bytes = g_file_load_bytes(file, NULL, NULL, NULL);
	if ( bytes == NULL ) return NULL;

	text = g_bytes_get_data(bytes, &len);
	n = storycode_parse_presentation(text);
	g_bytes_unref(bytes);
	if ( n == NULL ) return NULL;

	imagestore_set_parent(n->imagestore, g_file_get_parent(file));
	return n;
}


int narrative_save(Narrative *n, GFile *file)
{
	GFileOutputStream *fh;
	int r;
	GError *error = NULL;

	if ( file == NULL ) {
		fprintf(stderr, "Saving to NULL!\n");
		return 1;
	}

	fh = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
	if ( fh == NULL ) {
		fprintf(stderr, _("Open failed: %s\n"), error->message);
		return 1;
	}
	r = storycode_write_presentation(n, G_OUTPUT_STREAM(fh));
	if ( r ) {
		fprintf(stderr, _("Couldn't save presentation\n"));
	}
	g_object_unref(fh);

	imagestore_set_parent(n->imagestore, g_file_get_parent(file));

	n->saved = 1;
	return 0;
}


void narrative_set_unsaved(Narrative *n)
{
	n->saved = 0;
}


int narrative_get_unsaved(Narrative *n)
{
	return !n->saved;
}


void narrative_add_stylesheet(Narrative *n, Stylesheet *ss)
{
	assert(n->stylesheet == NULL);
	n->stylesheet = ss;
}


Stylesheet *narrative_get_stylesheet(Narrative *n)
{
	if ( n == NULL ) return NULL;
	return n->stylesheet;
}


const char *narrative_get_language(Narrative *n)
{
	if ( n == NULL ) return NULL;
	return n->language;
}


ImageStore *narrative_get_imagestore(Narrative *n)
{
	if ( n == NULL ) return NULL;
	return n->imagestore;
}


static void init_item(struct narrative_item *item)
{
	item->layout = NULL;
	item->text = NULL;
	item->slide = NULL;
	item->slide_thumbnail = NULL;
}


static struct narrative_item *add_item(Narrative *n)
{
	struct narrative_item *new_items;
	struct narrative_item *item;
	new_items = realloc(n->items, (n->n_items+1)*sizeof(struct narrative_item));
	if ( new_items == NULL ) return NULL;
	n->items = new_items;
	item = &n->items[n->n_items++];
	init_item(item);
	return item;
}


static struct narrative_item *insert_item(Narrative *n, int pos)
{
	add_item(n);
	memmove(&n->items[pos+1], &n->items[pos],
	        (n->n_items-pos-1)*sizeof(struct narrative_item));
	init_item(&n->items[pos+1]);
	return &n->items[pos+1];
}


void narrative_add_prestitle(Narrative *n, char *text)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_PRESTITLE;
	item->text = text;
	item->align = ALIGN_LEFT;
	item->layout = NULL;
}


void narrative_add_bp(Narrative *n, char *text)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_BP;
	item->text = text;
	item->align = ALIGN_LEFT;
	item->layout = NULL;
}


void narrative_add_text(Narrative *n, char *text)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_TEXT;
	item->text = text;
	item->align = ALIGN_LEFT;
	item->layout = NULL;
}


void narrative_add_slide(Narrative *n, Slide *slide)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_SLIDE;
	item->slide = slide;
	item->slide_thumbnail = NULL;
}


void narrative_insert_slide(Narrative *n, Slide *slide, int pos)
{
	struct narrative_item *item = insert_item(n, pos-1);
	item->type = NARRATIVE_ITEM_SLIDE;
	item->slide = slide;
	item->slide_thumbnail = NULL;
}


static void delete_item(Narrative *n, int del)
{
	int i;
	narrative_item_destroy(&n->items[del]);
	for ( i=del; i<n->n_items-1; i++ ) {
		n->items[i] = n->items[i+1];
	}
	n->n_items--;
}


/* Delete from item i1 offset o1 to item i2 offset o2, inclusive */
void narrative_delete_block(Narrative *n, int i1, size_t o1, int i2, size_t o2)
{
	int i;
	int n_del = 0;
	int merge = 1;

	/* Starting item */
	if ( n->items[i1].type == NARRATIVE_ITEM_SLIDE ) {
		delete_item(n, i1);
		i1--;
		i2--;
		merge = 0;
	} else {
		if ( i1 == i2 ) {
			memmove(&n->items[i1].text[o1],
			        &n->items[i1].text[o2],
			        strlen(n->items[i1].text)-o2+1);
			return;  /* easy case */
		} else {
			n->items[i1].text[o1] = '\0';
		}
	}

	/* Middle items */
	for ( i=i1+1; i<i2; i++ ) {
		/* Deleting the item moves all the subsequent items up, so the
		 * index to be deleted doesn't change. */
		delete_item(n, i1+1);
		n_del++;
	}
	i2 -= n_del;

	/* Last item */
	if ( n->items[i2].type == NARRATIVE_ITEM_SLIDE ) {
		delete_item(n, i2);
		merge = 0;
	} else {
		memmove(&n->items[i2].text[0],
		        &n->items[i2].text[o2],
		        strlen(&n->items[i2].text[o2])+1);
	}

	/* If the start and end points are in different paragraphs, and both
	 * of them are text (any kind), merge them.  Note that at this point,
	 * we know that i1 and i2 are different because otherwise we would've
	 * returned earlier ("easy case"). */
	assert(i1 != i2);
	if ( merge ) {
		char *new_text;
		size_t len = strlen(n->items[i1].text);
		len += strlen(n->items[i2].text);
		new_text = malloc(len+1);
		if ( new_text == NULL ) return;
		strcpy(new_text, n->items[i1].text);
		strcat(new_text, n->items[i2].text);
		free(n->items[i1].text);
		n->items[i1].text = new_text;
		delete_item(n, i2);
	}
}


void narrative_split_item(Narrative *n, int i1, size_t o1)
{
	struct narrative_item *item1;
	struct narrative_item *item2;

	item1 = &n->items[i1];
	item2 = insert_item(n, i1);

	if ( item1->type != NARRATIVE_ITEM_SLIDE ) {
		item2->text = strdup(&item1->text[o1]);
		item1->text[o1] = '\0';
	} else {
		item2->text = strdup("");
	}

	item2->type = NARRATIVE_ITEM_TEXT;
}


int narrative_get_num_items(Narrative *n)
{
	return n->n_items;
}


int narrative_get_num_slides(Narrative *n)
{
	int i;
	int ns = 0;
	for ( i=0; i<n->n_items; i++ ) {
		if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) ns++;
	}
	return ns;
}


Slide *narrative_get_slide(Narrative *n, int para)
{
	if ( para >= n->n_items ) return NULL;
	if ( n->items[para].type != NARRATIVE_ITEM_SLIDE ) return NULL;
	return n->items[para].slide;
}


int narrative_get_slide_number_for_para(Narrative *n, int para)
{
	int i;
	int ns = 0;
	for ( i=0; i<para; i++ ) {
		if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) ns++;
	}
	return ns;
}


int narrative_get_slide_number_for_slide(Narrative *n, Slide *s)
{
	int i;
	int ns = 0;
	for ( i=0; i<n->n_items; i++ ) {
		if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) {
			if ( n->items[i].slide == s ) return ns;
			ns++;
		}
	}
	return n->n_items;
}


Slide *narrative_get_slide_by_number(Narrative *n, int pos)
{
	int i;
	int ns = 0;
	for ( i=0; i<n->n_items; i++ ) {
		if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) {
			if ( ns == pos ) return n->items[i].slide;
			ns++;
		}
	}
	return NULL;
}
