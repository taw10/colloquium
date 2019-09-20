/*
 * slide.c
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
#include <stdio.h>
#include <assert.h>

#ifdef HAVE_PANGO
#include <pango/pangocairo.h>
#endif

#include "slide.h"
#include "slide_priv.h"


Slide *slide_new()
{
	Slide *s;
	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;
	s->n_items = 0;
	s->items = NULL;
	s->logical_w = -1.0;
	s->logical_h = -1.0;
	return s;
}

void slide_free(Slide *s)
{
	free(s->items);
	free(s);
}


void slide_delete_item(Slide *s, SlideItem *item)
{
	int i;
	for ( i=0; i<s->n_items; i++ ) {
		if ( &s->items[i] == item ) {
			memmove(&s->items[i], &s->items[i+1],
			        (s->n_items-i-1)*sizeof(SlideItem));
			s->n_items--;
			return;
		}
	}
	fprintf(stderr, "Didn't find slide item to delete!\n");
}


static SlideItem *add_item(Slide *s)
{
	SlideItem *new_items;
	SlideItem *item;

	new_items = realloc(s->items, (s->n_items+1)*sizeof(SlideItem));
	if ( new_items == NULL ) return NULL;
	s->items = new_items;
	item = &s->items[s->n_items++];

	item->paras = NULL;

	return item;
}


SlideItem *slide_add_image(Slide *s, char *filename, struct frame_geom geom)
{
	SlideItem *item;

	item = add_item(s);
	if ( item == NULL ) return NULL;

	item->type = SLIDE_ITEM_IMAGE;
	item->geom = geom;
	item->filename = filename;

	return item;
}


/* paras: array of arrays of text runs
 * n_runs: array of numbers of runs in each paragraph
 * n_paras: the number of paragraphs
 *
 * Will take ownership of the arrays of text runs, but not the array of arrays
 * Will NOT take ownership of the array of numbers of runs
 */
static SlideItem *add_text_item(Slide *s, struct text_run **paras, int *n_runs, int n_paras,
                                struct frame_geom geom, enum alignment alignment,
                                enum slide_item_type slide_item)
{
	int i;
	SlideItem *item;

	item = add_item(s);
	if ( item == NULL ) return NULL;

	item->type = slide_item;
	item->paras = malloc(n_paras*sizeof(struct slide_text_paragraph));
	if ( item->paras == NULL ) {
		s->n_items--;
		return NULL;
	}
	item->n_paras = n_paras;

	for ( i=0; i<n_paras; i++ ) {
		item->paras[i].runs = paras[i];
		item->paras[i].n_runs = n_runs[i];
		item->paras[i].layout = NULL;
	}

	item->geom = geom;
	item->align = alignment;

	return item;
}


int slide_add_footer(Slide *s)
{
	SlideItem *item;

	item = add_item(s);
	if ( item == NULL ) return 1;

	item->type = SLIDE_ITEM_FOOTER;

	return 0;
}


SlideItem *slide_add_text(Slide *s, struct text_run **paras, int *n_runs, int n_paras,
                          struct frame_geom geom, enum alignment alignment)
{
	return add_text_item(s, paras, n_runs, n_paras, geom, alignment,
	                     SLIDE_ITEM_TEXT);
}


SlideItem *slide_add_slidetitle(Slide *s, struct text_run **paras, int *n_runs, int n_paras)
{
	struct frame_geom geom;

	/* This geometry should never get used by the renderer */
	geom.x.len = 0.0;
	geom.x.unit = LENGTH_FRAC;
	geom.y.len = 0.0;
	geom.y.unit = LENGTH_FRAC;
	geom.w.len = 1.0;
	geom.w.unit = LENGTH_FRAC;
	geom.h.len = 1.0;
	geom.h.unit = LENGTH_FRAC;

	return add_text_item(s, paras, n_runs, n_paras, geom, ALIGN_INHERIT,
	                     SLIDE_ITEM_SLIDETITLE);
}


SlideItem *slide_add_prestitle(Slide *s, struct text_run **paras, int *n_runs, int n_paras)
{
	struct frame_geom geom;

	/* This geometry should never get used by the renderer */
	geom.x.len = 0.0;
	geom.x.unit = LENGTH_FRAC;
	geom.y.len = 0.0;
	geom.y.unit = LENGTH_FRAC;
	geom.w.len = 1.0;
	geom.w.unit = LENGTH_FRAC;
	geom.h.len = 1.0;
	geom.h.unit = LENGTH_FRAC;

	return add_text_item(s, paras, n_runs, n_paras, geom, ALIGN_INHERIT,
	                     SLIDE_ITEM_PRESTITLE);
}


static char units(enum length_unit u)
{
	if ( u == LENGTH_UNIT ) return 'u';
	if ( u == LENGTH_FRAC ) return 'f';
	return '?';
}


void describe_slide(Slide *s)
{
	int i;

	printf("  %i items\n", s->n_items);
	for ( i=0; i<s->n_items; i++ ) {
		printf("item %i: %i\n", i, s->items[i].type);
		printf("geom %f %c x %f %c + %f %c + %f %c\n",
	               s->items[i].geom.x.len, units(s->items[i].geom.x.unit),
	               s->items[i].geom.y.len, units(s->items[i].geom.y.unit),
	               s->items[i].geom.w.len, units(s->items[i].geom.w.unit),
	               s->items[i].geom.h.len, units(s->items[i].geom.h.unit));
	}
}


int slide_set_logical_size(Slide *s, double w, double h)
{
	if ( s == NULL ) return 1;
	s->logical_w = w;
	s->logical_h = h;
	return 0;
}


int slide_get_logical_size(Slide *s, Stylesheet *ss, double *w, double *h)
{
	if ( s == NULL ) return 1;

	if ( s->logical_w < 0.0 ) {
		/* Slide-specific value not set, use stylesheet */
		return stylesheet_get_slide_default_size(ss, w, h);
	}

	*w = s->logical_w;
	*h = s->logical_h;
	return 0;
}


static const char *style_name_for_slideitem(enum slide_item_type t)
{
	switch ( t ) {

		case SLIDE_ITEM_TEXT :
		return "SLIDE.TEXT";

		case SLIDE_ITEM_IMAGE :
		return "SLIDE.IMAGE";

		case SLIDE_ITEM_PRESTITLE :
		return "SLIDE.PRESTITLE";

		case SLIDE_ITEM_SLIDETITLE :
		return "SLIDE.SLIDETITLE";

		case SLIDE_ITEM_FOOTER :
		return "SLIDE.FOOTER";

	}

	fprintf(stderr, "Invalid slide item %i\n", t);
	return "SLIDE.TEXT";
}


static double lcalc(struct length l, double pd)
{
	if ( l.unit == LENGTH_UNIT ) {
		return l.len;
	} else {
		return l.len * pd;
	}
}


void slide_item_get_geom(SlideItem *item, Stylesheet *ss,
                         double *x, double *y, double *w, double *h,
                         double slide_w, double slide_h)
{
	struct frame_geom geom;

	if ( (item->type == SLIDE_ITEM_TEXT)
	  || (item->type == SLIDE_ITEM_IMAGE) )
	{
		geom = item->geom;
	} else {
		if ( stylesheet_get_geometry(ss, style_name_for_slideitem(item->type), &geom) ) {
			*x = 0.0;  *y = 0.0;
			*w = 0.0;  *h = 0.0;
			return;
		}
	}

	*x = lcalc(geom.x, slide_w);
	*y = lcalc(geom.y, slide_h);
	*w = lcalc(geom.w, slide_w);
	*h = lcalc(geom.h, slide_h);
}


void slide_item_get_padding(SlideItem *item, Stylesheet *ss,
                            double *l, double *r, double *t, double *b,
                            double slide_w, double slide_h)
{
	struct length padding[4];
	double frx, fry, frw, frh;

	if ( stylesheet_get_padding(ss, style_name_for_slideitem(item->type), padding) ) {
		*l = 0.0;  *r = 0.0;  *t = 0.0;  *b = 0.0;
		return;
	}

	slide_item_get_geom(item, ss, &frx, &fry, &frw, &frh, slide_w, slide_h);

	*l = lcalc(padding[0], frw);
	*r = lcalc(padding[1], frh);
	*t = lcalc(padding[2], frw);
	*b = lcalc(padding[3], frh);
}


void slide_item_split_text_paragraph(SlideItem *item, int para, size_t off)
{
#if 0
	struct slide_text_paragraph *np;

	np = realloc(item->paras, (item->n_paras+1)*sizeof(struct slide_text_paragraph));
	if ( np == NULL ) return;

	item->paras = np;
	item->n_paras++;

	memmove(&item->paras[para+1], &item->paras[para],
	        (item->n_paras - para - 1)*sizeof(struct slide_text_paragraph));

	item->paras[para+1].text = strdup(&item->paras[para].text[off]);
	item->paras[para+1].layout = NULL;
	item->paras[para].text[off] = '\0';
#endif
}


static void delete_paragraph(SlideItem *item, int del)
{
#if 0
	int i;

#ifdef HAVE_PANGO
	g_object_unref(item->paras[del].layout);
#endif
	free(item->paras[del].text);

	for ( i=del; i<item->n_paras-1; i++ ) {
		item->paras[i] = item->paras[i+1];
	}
	item->n_paras--;
#endif
}


void slide_item_delete_text(SlideItem *item, int i1, size_t o1, int i2, size_t o2)
{
#if 0
	int i;
	int n_del = 0;

	/* Starting item */
	if ( i1 == i2 ) {
		memmove(&item->paras[i1].text[o1],
		        &item->paras[i1].text[o2],
		        strlen(item->paras[i1].text)-o2+1);
		return;  /* easy case */
	} else {
		item->paras[i1].text[o1] = '\0';
	}

	/* Middle items */
	for ( i=i1+1; i<i2; i++ ) {
		/* Deleting the item moves all the subsequent items up, so the
		 * index to be deleted doesn't change. */
		delete_paragraph(item, i1+1);
		n_del++;
	}
	i2 -= n_del;

	/* Last item */
	memmove(&item->paras[i2].text[0],
	        &item->paras[i2].text[o2],
	        strlen(&item->paras[i2].text[o2])+1);

	assert(i1 != i2);
	char *new_text;
	size_t len = strlen(item->paras[i1].text);
	len += strlen(item->paras[i2].text);
	new_text = malloc(len+1);
	if ( new_text == NULL ) return;
	strcpy(new_text, item->paras[i1].text);
	strcat(new_text, item->paras[i2].text);
	free(item->paras[i1].text);
	item->paras[i1].text = new_text;
	delete_paragraph(item, i2);
#endif
}
