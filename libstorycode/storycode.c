/*
 * storycode.c
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
#include <gio/gio.h>

#include "narrative.h"
#include "slide.h"
#include "stylesheet.h"
#include "narrative_priv.h"
#include "slide_priv.h"

#include "storycode_parse.h"
#include "storycode_lex.h"


extern int scdebug;


Narrative *storycode_parse_presentation(const char *sc)
{
	YY_BUFFER_STATE b;
	Narrative *n;

	//BEGIN(0);
	b = sc_scan_string(sc);
	scdebug = 1;
	n = narrative_new();
	scparse(n);
	sc_delete_buffer(b);
	//narrative_debug(n);

	return n;
}


static int write_string(GOutputStream *fh, char *str)
{
	gssize r;
	GError *error = NULL;
	r = g_output_stream_write(fh, str, strlen(str), NULL, &error);
	if ( r == -1 ) {
		fprintf(stderr, "Write failed: %s\n", error->message);
		return 1;
	}
	return 0;
}


char unitc(enum length_unit unit)
{
	if ( unit == LENGTH_FRAC ) return 'f';
	if ( unit == LENGTH_UNIT ) return 'u';
	return '?';
}


const char *bgcolc(enum gradient bggrad)
{
	if ( bggrad == GRAD_NONE ) return "";
	if ( bggrad == GRAD_HORIZ ) return "HORIZONTAL ";
	if ( bggrad == GRAD_VERT ) return "VERTICAL ";
	return "?";
}


const char *alignc(enum alignment ali)
{
	if ( ali == ALIGN_LEFT ) return "left";
	if ( ali == ALIGN_CENTER ) return "center";
	if ( ali == ALIGN_RIGHT ) return "right";
	return "?";
}


static const char *maybe_alignment(enum alignment ali)
{
	if ( ali == ALIGN_INHERIT ) return "";
	if ( ali == ALIGN_LEFT ) return "[left]";
	if ( ali == ALIGN_CENTER ) return "[center]";
	if ( ali == ALIGN_RIGHT ) return "[right]";
	return "[?]";
}


static void write_run_border(GOutputStream *fh, enum text_run_type t)
{
	if ( t == TEXT_RUN_BOLD ) write_string(fh, "*");
	if ( t == TEXT_RUN_ITALIC ) write_string(fh, "/");
	if ( t == TEXT_RUN_UNDERLINE ) write_string(fh, "_");
}


static char *escape_text(const char *in, size_t len)
{
	int i, j;
	size_t nl = 0;
	size_t np = 0;
	const char *esc = "*/_";
	size_t n_esc = 3;
	char *out;

	for ( i=0; i<len; i++ ) {
		for ( j=0; j<n_esc; j++ ) {
			if ( in[i] == esc[j] ) {
				nl++;
				break;
			}
		}
	}

	out = malloc(len + nl + 1);
	if ( out == NULL ) return NULL;

	np = 0;
	for ( i=0; i<len; i++ ) {
		for ( j=0; j<n_esc; j++ ) {
			if ( in[i] == esc[j] ) {
				out[np++] = '\\';
				break;
			}
		}
		out[np++] = in[i];
	}
	out[np] = '\0';

	return out;
}


static void write_partial_run(GOutputStream *fh, struct text_run *run, size_t start, size_t len)
{
	char *escaped_str;
	write_run_border(fh, run->type);
	escaped_str = escape_text(run->text+start, len);
	write_string(fh, escaped_str);
	free(escaped_str);
	write_run_border(fh, run->type);
}


static void write_para(GOutputStream *fh, struct text_run *runs, int n_runs)
{
	int i;
	for ( i=0; i<n_runs; i++ ) {
		write_partial_run(fh, &runs[i], 0, strlen(runs[i].text));
	}
}


static void write_text(GOutputStream *fh, SlideItem *item, int geom,
                       const char *t)
{
	char tmp[256];
	size_t indent;
	int i;

	if ( geom ) {
		snprintf(tmp, 255, "  %s[%.4g%cx%.4g%c+%.4g%c+%.4g%c]%s", t,
		         item->geom.w.len, unitc(item->geom.w.unit),
		         item->geom.h.len, unitc(item->geom.h.unit),
		         item->geom.x.len, unitc(item->geom.x.unit),
		         item->geom.y.len, unitc(item->geom.y.unit),
		         maybe_alignment(item->align));
	} else {
		snprintf(tmp, 255, "  %s%s",
		         t, maybe_alignment(item->align));
	}

	indent = strlen(tmp);
	write_string(fh, tmp);
	write_string(fh, ": ");
	write_para(fh, item->paras[0].runs, item->paras[0].n_runs);
	write_string(fh, "\n");
	for ( i=0; i<indent; i++ ) tmp[i] = ' ';
	for ( i=1; i<item->n_paras; i++ ) {
		write_string(fh, tmp);
		write_string(fh, ": ");
		write_para(fh, item->paras[i].runs, item->paras[i].n_runs);
		write_string(fh, "\n");
	}
}


static void write_image(GOutputStream *fh, SlideItem *item)
{
	char tmp[256];

	snprintf(tmp, 255, "  IMAGE[%.4g%cx%.4g%c+%.4g%c+%.4g%c]",
	         item->geom.w.len, unitc(item->geom.w.unit),
	         item->geom.h.len, unitc(item->geom.h.unit),
	         item->geom.x.len, unitc(item->geom.x.unit),
	         item->geom.y.len, unitc(item->geom.y.unit));

	write_string(fh, tmp);
	write_string(fh, ": ");
	write_string(fh, item->filename);
	write_string(fh, "\n");
}


static int write_slide(GOutputStream *fh, Slide *s)
{
	int i;

	for ( i=0; i<s->n_items; i++ ) {
		switch ( s->items[i].type ) {

			case SLIDE_ITEM_TEXT:
			write_text(fh, &s->items[i], 1, "TEXT");
			break;

			case SLIDE_ITEM_PRESTITLE:
			write_text(fh, &s->items[i], 0, "PRESTITLE");
			break;

			case SLIDE_ITEM_SLIDETITLE:
			write_text(fh, &s->items[i], 0, "SLIDETITLE");
			break;

			case SLIDE_ITEM_FOOTER:
			write_string(fh, "  FOOTER\n");
			break;

			case SLIDE_ITEM_IMAGE:
			write_image(fh, &s->items[i]);
			break;

		}
	}

	return 0;
}


static int write_starter(GOutputStream *fh, struct narrative_item *item)
{
	switch ( item->type ) {

		case NARRATIVE_ITEM_TEXT:
		/* FIXME: separate alignment */
		if ( write_string(fh, ": ") ) return 1;
		break;

		case NARRATIVE_ITEM_PRESTITLE:
		/* FIXME: separate alignment */
		if ( write_string(fh, "PRESTITLE: ") ) return 1;
		break;

		case NARRATIVE_ITEM_BP:
		/* FIXME: separate alignment */
		if ( write_string(fh, "BP: ") ) return 1;
		break;

		case NARRATIVE_ITEM_SLIDE:
		/* FIXME: separate slide size */
		if ( write_string(fh, "SLIDE ") ) return 1;
		break;

		case NARRATIVE_ITEM_EOP:
		if ( write_string(fh, "ENDOFPRESENTATION") ) return 1;
		break;

	}
	return 0;
}


static int write_item(GOutputStream *fh, struct narrative_item *item)
{
	if ( write_starter(fh, item) ) return 1;
	switch ( item->type ) {

		case NARRATIVE_ITEM_TEXT:
		write_para(fh, item->runs, item->n_runs);
		break;

		case NARRATIVE_ITEM_PRESTITLE:
		write_para(fh, item->runs, item->n_runs);
		break;

		case NARRATIVE_ITEM_BP:
		write_para(fh, item->runs, item->n_runs);
		break;

		case NARRATIVE_ITEM_SLIDE:
		if ( write_string(fh, "{\n") ) return 1;
		if ( write_slide(fh, item->slide) ) return 1;
		if ( write_string(fh, "}") ) return 1;
		break;

		case NARRATIVE_ITEM_EOP:
		break;

	}
	return 0;
}


int narrative_write_item(Narrative *n, int item, GOutputStream *fh)
{
	return write_item(fh, &n->items[item]);
}


int narrative_write_partial_item(Narrative *n, int inum, size_t start, ssize_t end,
                                 GOutputStream *fh)
{
	struct narrative_item *item;
	int r1, r2;
	size_t r1offs, r2offs;
	size_t r1len, r2len;
	int r;

	if ( !narrative_item_is_text(n, inum) ) return narrative_write_item(n, inum, fh);

	item = &n->items[inum];
	r1 = narrative_which_run(item, start, &r1offs);

	if ( end >= 0 ) {
		r2 = narrative_which_run(item, end, &r2offs);
	} else {
		r2 = item->n_runs - 1;
	}

	r1len = strlen(item->runs[r1].text);
	r2len = strlen(item->runs[r2].text);

	if ( end < 0 ) r2offs = r2len;

	if ( write_starter(fh, item) ) return 1;

	for ( r=r1; r<=r2; r++ ) {
		if ( (r1==r2) && (r==r1) ) {
			write_partial_run(fh, &item->runs[r], r1offs, r2offs - r1offs);
		} else if ( r == r1 ) {
			write_partial_run(fh, &item->runs[r], r1offs, r1len - r1offs);
		} else if ( r == r2 ) {
			write_partial_run(fh, &item->runs[r], 0, r2offs);
		} else {
			write_partial_run(fh, &item->runs[r], 0, strlen(item->runs[r].text));
		}
	}

	return 0;
}


int storycode_write_presentation(Narrative *n, GOutputStream *fh)
{
	int i;
	char *ss_text;

	/* Stylesheet */
	ss_text = stylesheet_serialise(n->stylesheet);
	if ( ss_text == NULL ) return 1;
	if ( write_string(fh, ss_text) ) return 1;

	if ( write_string(fh, "\n") ) return 1;

	for ( i=0; i<n->n_items; i++ ) {
		if ( write_item(fh, &n->items[i]) ) return 1;
		if ( write_string(fh, "\n") ) return 1;
	}

	return 0;
}
