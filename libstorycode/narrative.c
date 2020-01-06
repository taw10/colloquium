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
	n->stylesheet = stylesheet_new();
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
	int i;
	for ( i=0; i<item->n_runs; i++ ) {
		free(item->runs[i].text);
	}
	free(item->runs);
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


/* Free the narrative and all contents, but not the stylesheet */
void narrative_free(Narrative *n)
{
	int i;
	for ( i=0; i<n->n_items; i++ ) {
		narrative_item_destroy(&n->items[i]);
	}
	free(n->items);
	free(n);
}


void narrative_add_empty_item(Narrative *n)
{
	struct text_run *runs;

	runs = malloc(sizeof(struct text_run));
	if ( runs == NULL ) return;

	runs[0].text = strdup("");
	runs[0].type = TEXT_RUN_NORMAL;
	if ( runs[0].text == NULL ) {
		free(runs);
		return;
	}

	narrative_add_text(n, runs, 1);
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

	if ( n->n_items == 0 ) {
		/* Presentation is empty.  Add a dummy to start things off */
		narrative_add_empty_item(n);
	}

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


int narrative_item_is_text(Narrative *n, int item)
{
	if ( n->items[item].type == NARRATIVE_ITEM_SLIDE ) return 0;
	if ( n->items[item].type == NARRATIVE_ITEM_EOP ) return 0;
	return 1;
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
#ifdef HAVE_PANGO
	item->layout = NULL;
#endif
	item->runs = NULL;
	item->n_runs = 0;
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


/* New item will have index 'pos' */
static struct narrative_item *insert_item(Narrative *n, int pos)
{
	add_item(n);
	memmove(&n->items[pos+1], &n->items[pos],
	        (n->n_items-pos-1)*sizeof(struct narrative_item));
	init_item(&n->items[pos]);
	return &n->items[pos];
}


/* The new text item takes ownership of the array of runs */
extern void add_text_item(Narrative *n, struct text_run *runs, int n_runs,
                          enum narrative_item_type type)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = type;
	item->align = ALIGN_INHERIT;
	item->layout = NULL;

	item->runs = runs;
	item->n_runs = n_runs;
}


void narrative_add_text(Narrative *n, struct text_run *runs, int n_runs)
{
	add_text_item(n, runs, n_runs, NARRATIVE_ITEM_TEXT);
}


void narrative_add_prestitle(Narrative *n, struct text_run *runs, int n_runs)
{
	add_text_item(n, runs, n_runs, NARRATIVE_ITEM_PRESTITLE);
}


void narrative_add_bp(Narrative *n, struct text_run *runs, int n_runs)
{
	add_text_item(n, runs, n_runs, NARRATIVE_ITEM_BP);
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


void narrative_add_eop(Narrative *n)
{
	struct narrative_item *item;

	item = add_item(n);
	if ( item == NULL ) return;

	item->type = NARRATIVE_ITEM_EOP;
}


void narrative_insert_slide(Narrative *n, Slide *slide, int pos)
{
	struct narrative_item *item = insert_item(n, pos);
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


/* o2 = -1 means 'to the end' */
static void delete_text(struct narrative_item *item, size_t o1, ssize_t o2)
{
	int r1, r2;
	size_t roffs1, roffs2;

	r1 = narrative_which_run(item, o1, &roffs1);

	/* This means 'delete to end' */
	if ( o2 == -1 ) {
		int i;
		o2 = 0;
		for ( i=0; i<item->n_runs; i++ ) {
			o2 += strlen(item->runs[i].text);
		}
	}

	r2 = narrative_which_run(item, o2, &roffs2);

	if ( r1 == r2 ) {

		/* Easy case */
		memmove(&item->runs[r1].text[roffs1],
		        &item->runs[r2].text[roffs2],
		        strlen(item->runs[r1].text)-roffs2+1);

	} else {

		int n_middle;

		/* Truncate the first run */
		item->runs[r1].text[roffs1] = '\0';

		/* Delete any middle runs */
		n_middle = r2 - r1 - 1;
		if ( n_middle > 0 ) {
			memmove(&item->runs[r1+1], &item->runs[r2],
			        (item->n_runs-r2)*sizeof(struct text_run));
			item->n_runs -= n_middle;
			r2 -= n_middle;
		}

		/* Last run */
		memmove(item->runs[r2].text, &item->runs[r2].text[roffs2],
		        strlen(&item->runs[r2].text[roffs2])+1);
	}
}


/* Delete from item i1 offset o1 to item i2 offset o2, inclusive */
void narrative_delete_block(Narrative *n, int i1, size_t o1, int i2, size_t o2)
{
	int i;
	int n_del = 0;
	int merge = 1;
	int middle;    /* This is where the "middle deletion" will begin */

	/* Starting item */
	if ( !narrative_item_is_text(n, i1) ) {
		delete_item(n, i1);
		if ( i1 == i2 ) return;  /* only one slide to delete */
		middle = i1; /* ... which is now the item just after the slide */
		i2--;
		merge = 0;
	} else {
		if ( i1 == i2 ) {
			delete_text(&n->items[i1], o1, o2);
			return;  /* easy case */
		} else {
			/* Truncate i1 at o1 */
			delete_text(&n->items[i1], o1, -1);
			middle = i1+1;
		}
	}

	/* Middle items */
	for ( i=middle; i<i2; i++ ) {
		/* Deleting the item moves all the subsequent items up, so the
		 * index to be deleted doesn't change. */
		delete_item(n, middle);
		n_del++;
	}
	i2 -= n_del;

	/* Last item */
	if ( !narrative_item_is_text(n, i2) ) {
		delete_item(n, i2);
		return;
	}

	/* We end with a text item */
	delete_text(&n->items[i2], 0, o2);

	/* If the start and end points are in different paragraphs, and both
	 * of them are text (any kind), merge them.  Note that at this point,
	 * we know that i1 and i2 are different because otherwise we
	 * would've returned earlier ("easy case"). */
	if ( merge ) {

		struct text_run *i1newruns;
		struct narrative_item *item1;
		struct narrative_item *item2;

		assert(i1 != i2);
		item1 = &n->items[i1];
		item2 = &n->items[i2];

		i1newruns = realloc(item1->runs,
		                    (item1->n_runs+item2->n_runs)*sizeof(struct text_run));
		if ( i1newruns == NULL ) return;
		item1->runs = i1newruns;

		for ( i=0; i<item2->n_runs; i++ ) {
			item1->runs[i+item1->n_runs].text = strdup(item2->runs[i].text);
			item1->runs[i+item1->n_runs].type = item2->runs[i].type;
		}
		item1->n_runs += item2->n_runs;

		delete_item(n, i2);
	}
}


int narrative_which_run(struct narrative_item *item, size_t item_offs, size_t *run_offs)
{
	int run;
	size_t pos = 0;

	for ( run=0; run<item->n_runs; run++ ) {
		size_t npos = pos + strlen(item->runs[run].text);
		if ( npos >= item_offs ) break;
		pos = npos;
	}
	if ( run_offs != NULL ) {
		*run_offs = item_offs - pos;
	}
	return run;
}


void narrative_split_item(Narrative *n, int i1, size_t o1)
{
	struct narrative_item *item1;
	struct narrative_item *item2;

	item2 = insert_item(n, i1+1);
	item1 = &n->items[i1];  /* NB n->items was realloced by insert_item */
	item2->type = NARRATIVE_ITEM_TEXT;

	if ( narrative_item_is_text(n, i1) ) {

		size_t run_offs;
		int run = narrative_which_run(item1, o1, &run_offs);
		int j;

		item2->n_runs = item1->n_runs - run;
		item2->runs = malloc(item2->n_runs*sizeof(struct text_run));
		for ( j=run; j<item1->n_runs; j++ ) {
			item2->runs[j-run] = item1->runs[j];
		}

		/* Now break the run */
		item2->runs[0].text = strdup(item1->runs[run].text+run_offs);
		item1->runs[run].text[run_offs] = '\0';
		item1->n_runs = run + 1;

	} else {

		/* Splitting a non-text run simply means creating a new
		 * plain text item after it */
		item2->runs = malloc(sizeof(struct text_run));
		item2->n_runs = 1;
		item2->runs[0].text = strdup("");
		item2->runs[0].type = TEXT_RUN_NORMAL;;

	}
}


int narrative_get_num_items(Narrative *n)
{
	return n->n_items;
}


int narrative_get_num_items_to_eop(Narrative *n)
{
	int i;
	for ( i=0; i<n->n_items; i++ ) {
		if ( n->items[i].type == NARRATIVE_ITEM_EOP ) return i;
	}
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


/* Return the text between item p1/offset o1 and p2/o2 */
char *narrative_range_as_storycode(Narrative *n, int p1, size_t o1, int p2, size_t o2)
{
	GOutputStream *fh;
	char *text;
	int i;

	fh = g_memory_output_stream_new_resizable();
	if ( fh == NULL ) return NULL;

	for ( i=p1; i<=p2; i++ ) {

		if ( ((i>p1) && (i<p2)) || !narrative_item_is_text(n, i) ) {

			narrative_write_item(n, i, fh);

		} else {

			size_t start_offs, end_offs;
			GError *error = NULL;

			if ( i==p1 ) {
				start_offs = o1;
			} else {
				start_offs = 0;
			}

			if ( i==p2 ) {
				end_offs = o2;
			} else {
				end_offs = -1;
			}

			narrative_write_partial_item(n, i, start_offs, end_offs, fh);
			if ( i < p2 ) g_output_stream_write(fh, "\n", 1, NULL, &error);

		}

	}

	g_output_stream_close(fh, NULL, NULL);
	text = g_memory_output_stream_steal_data(G_MEMORY_OUTPUT_STREAM(fh));
	g_object_unref(fh);

	return text;
}


/* Return the text between item p1/offset o1 and p2/o2 */
char *narrative_range_as_text(Narrative *n, int p1, size_t o1, int p2, size_t o2)
{
	int i;
	char *t;
	int r1, r2;
	size_t r1offs, r2offs;
	size_t len = 0;
	size_t size = 256;

	t = malloc(size);
	if ( t == NULL ) return NULL;
	t[0] = '\0';

	r1 = narrative_which_run(&n->items[p1], o1, &r1offs);
	r2 = narrative_which_run(&n->items[p2], o2, &r2offs);

	for ( i=p1; i<=p2; i++ ) {

		int r;

		/* Skip non-text runs straight away */
		if ( !narrative_item_is_text(n, i) ) continue;

		for ( r=0; r<n->items[i].n_runs; r++ ) {

			size_t run_text_len, len_to_add, start_run_offs;

			/* Is this run within the range? */
			if ( (i==p1) && (r<r1) ) continue;
			if ( (i==p2) && (r>r2) ) continue;

			run_text_len = strlen(n->items[i].runs[r].text);

			if ( (p1==p2) && (r1==r2) && (r==r1) ) {
				start_run_offs = r1offs;
				len_to_add = r2offs - r1offs;
			} else if ( (i==p1) && (r==r1) ) {
				start_run_offs = r1offs;
				len_to_add = run_text_len - r1offs;
			} else if ( (i==p2) && (r==r2) ) {
				start_run_offs = 0;
				len_to_add = r2offs;
			} else {
				start_run_offs = 0;
				len_to_add = run_text_len;
			}

			if ( len_to_add == 0 ) continue;

			/* Ensure space */
			if ( len + len_to_add + 2 > size ) {
				char *nt;
				size += 256 + len_to_add;
				nt = realloc(t, size);
				if ( nt == NULL ) {
					fprintf(stderr, "Failed to allocate\n");
					return NULL;
				}
				t = nt;
			}

			memcpy(&t[len], n->items[i].runs[r].text+start_run_offs,
			       len_to_add);
			len += len_to_add;
			t[len] = '\0';
		}

		if ( i < p2 ) {
			t[len++] = '\n';
			t[len] = '\0';
		}

	}

	return t;
}


static void debug_runs(struct narrative_item *item)
{
	int j;
	for ( j=0; j<item->n_runs; j++ ) {
		printf("Run %i: '%s'\n", j, item->runs[j].text);
	}
}


void narrative_debug(Narrative *n)
{
	int i;

	for ( i=0; i<n->n_items; i++ ) {

		struct narrative_item *item = &n->items[i];
		printf("Item %i ", i);
		switch ( item->type ) {

			case NARRATIVE_ITEM_TEXT :
			printf("(text):\n");
			debug_runs(item);
			break;

			case NARRATIVE_ITEM_BP :
			printf("(bp):\n");
			debug_runs(item);
			break;

			case NARRATIVE_ITEM_PRESTITLE :
			printf("(prestitle):\n");
			debug_runs(item);
			break;

			case NARRATIVE_ITEM_EOP :
			printf("(EOP marker)\n");
			break;

			case NARRATIVE_ITEM_SLIDE :
			printf("Slide:\n");
			describe_slide(item->slide);
			break;

		}

	}
}
