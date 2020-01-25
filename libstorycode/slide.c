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


static SlideItem *slide_item_new()
{
	SlideItem *item;
	item = malloc(sizeof(struct _slideitem));
	if ( item == NULL ) return NULL;
	item->paras = NULL;
	return item;
}


SlideItem *slide_add_item(Slide *s, SlideItem *item)
{
	SlideItem *new_items;

	new_items = realloc(s->items, (s->n_items+1)*sizeof(SlideItem));
	if ( new_items == NULL ) return NULL;
	s->items = new_items;

	/* Copy contents and free top-level */
	s->items[s->n_items++] = *item;
	free(item);
	return &s->items[s->n_items-1];
}


SlideItem *slide_item_image(char *filename, struct frame_geom geom)
{
	SlideItem *item;

	item = slide_item_new();
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
static SlideItem *text_item_new(struct text_run **paras, int *n_runs, int n_paras,
                                struct frame_geom geom, enum alignment alignment,
                                enum slide_item_type slide_item)
{
	int i;
	SlideItem *item;

	item = slide_item_new();
	if ( item == NULL ) return NULL;

	item->type = slide_item;
	item->paras = malloc(n_paras*sizeof(struct slide_text_paragraph));
	if ( item->paras == NULL ) {
		free(item);
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


SlideItem *slide_item_footer()
{
	SlideItem *item = slide_item_new();
	if ( item == NULL ) return NULL;
	item->type = SLIDE_ITEM_FOOTER;
	return item;
}


SlideItem *slide_item_text(struct text_run **paras, int *n_runs, int n_paras,
                           struct frame_geom geom, enum alignment alignment)
{
	return text_item_new(paras, n_runs, n_paras, geom, alignment,
	                     SLIDE_ITEM_TEXT);
}


SlideItem *slide_item_slidetitle(struct text_run **paras, int *n_runs, int n_paras)
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

	return text_item_new(paras, n_runs, n_paras, geom, ALIGN_INHERIT,
	                     SLIDE_ITEM_SLIDETITLE);
}


SlideItem *slide_item_prestitle(struct text_run **paras, int *n_runs, int n_paras)
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

	return text_item_new(paras, n_runs, n_paras, geom, ALIGN_INHERIT,
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


static struct slide_text_paragraph *insert_paragraph(SlideItem *item, int pos)
{
	struct slide_text_paragraph *paras_new;

	paras_new = realloc(item->paras, (item->n_paras+1)*sizeof(struct slide_text_paragraph));
	if ( paras_new == NULL ) return NULL;

	item->paras = paras_new;
	item->n_paras++;

	memmove(&item->paras[pos+1], &item->paras[pos],
	        (item->n_paras-pos-1)*sizeof(struct slide_text_paragraph));

	item->paras[pos].runs = NULL;
	item->paras[pos].layout = NULL;
	item->paras[pos].n_runs = 0;
	return &item->paras[pos];
}


void slide_item_split_text_paragraph(SlideItem *item, int para_num, size_t off)
{
	size_t run_offs;
	int j;
	struct slide_text_paragraph *para1;
	struct slide_text_paragraph *para2;
	int run = slide_which_run(&item->paras[para_num], off, &run_offs);

	para2 = insert_paragraph(item, para_num+1);
	para1 = &item->paras[para_num];  /* NB n->items was realloced by insert_item */

	para2->n_runs = para1->n_runs - run;
	para2->runs = malloc(para2->n_runs*sizeof(struct text_run));
	for ( j=run; j<para1->n_runs; j++ ) {
		para2->runs[j-run] = para1->runs[j];
	}

	/* Now break the run */
	para2->runs[0].text = strdup(para1->runs[run].text+run_offs);
	para1->runs[run].text[run_offs] = '\0';
	para1->n_runs = run + 1;
}


static void delete_paragraph(SlideItem *item, int del, int delete_runs)
{
	int i;

#ifdef HAVE_PANGO
	g_object_unref(item->paras[del].layout);
#endif

	if ( delete_runs ) {
		for ( i=0; i<item->paras[del].n_runs; i++ ) {
			free(item->paras[del].runs[i].text);
		}
	}

	for ( i=del; i<item->n_paras-1; i++ ) {
		item->paras[i] = item->paras[i+1];
	}
	item->n_paras--;
}


static void slide_paragraph_delete_text(struct slide_text_paragraph *para,
                                        size_t o1, ssize_t o2)
{
	int r1, r2;
	size_t roffs1, roffs2;

	r1 = slide_which_run(para, o1, &roffs1);

	/* This means 'delete to end' */
	if ( o2 == -1 ) {
		int i;
		o2 = 0;
		for ( i=0; i<para->n_runs; i++ ) {
			o2 += strlen(para->runs[i].text);
		}
	}

	r2 = slide_which_run(para, o2, &roffs2);

	if ( r1 == r2 ) {

		/* Easy case */
		memmove(&para->runs[r1].text[roffs1],
		        &para->runs[r2].text[roffs2],
		        strlen(para->runs[r1].text)-roffs2+1);

	} else {

		int n_middle;

		/* Truncate the first run */
		para->runs[r1].text[roffs1] = '\0';

		/* Delete any middle runs */
		n_middle = r2 - r1 - 1;
		if ( n_middle > 0 ) {
			memmove(&para->runs[r1+1], &para->runs[r2],
			        (para->n_runs-r2)*sizeof(struct text_run));
			para->n_runs -= n_middle;
			r2 -= n_middle;
		}

		/* Last run */
		memmove(para->runs[r2].text, &para->runs[r2].text[roffs2],
		        strlen(&para->runs[r2].text[roffs2])+1);
	}
}


void slide_item_delete_text(SlideItem *item, int p1, size_t o1, int p2, size_t o2)
{
	int i;
	struct text_run *new_runs;
	int n_del = 0;

	/* Starting paragraph */
	if ( p1 == p2 ) {
		slide_paragraph_delete_text(&item->paras[p1], o1, o2);
		return;  /* easy case */
	} else {
		slide_paragraph_delete_text(&item->paras[p1], o1, -1);
	}

	/* Middle items */
	for ( i=p1+1; i<p2; i++ ) {
		/* Deleting the item moves all the subsequent items up, so the
		 * index to be deleted doesn't change. */
		delete_paragraph(item, p1+1, 1);
		n_del++;
	}
	p2 -= n_del;

	/* Last item */
	slide_paragraph_delete_text(&item->paras[p2], 0, o2);

	/* Move runs from p2 to p1, then delete p2 */
	assert(p1 != p2);
	new_runs = realloc(item->paras[p1].runs,
	                   (item->paras[p1].n_runs+item->paras[p2].n_runs)*sizeof(struct text_run));
	if ( new_runs == NULL ) return;

	memcpy(&new_runs[item->paras[p1].n_runs], item->paras[p2].runs,
	       item->paras[p2].n_runs*sizeof(struct text_run));
	item->paras[p1].n_runs += item->paras[p2].n_runs;
	item->paras[p1].runs = new_runs;

	/* Delete the paragraph, but not the runs which we just copied */
	delete_paragraph(item, p2, 0);
}
