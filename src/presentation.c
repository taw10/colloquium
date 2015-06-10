/*
 * presentation.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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
#include <gtk/gtk.h>

#include "presentation.h"
#include "slide_window.h"
#include "frame.h"
#include "imagestore.h"
#include "wrap.h"
#include "notes.h"
#include "inhibit_screensaver.h"
#include "render.h"
#include "sc_interp.h"


void free_presentation(struct presentation *p)
{
	int i;
	int final = 0;

	for ( i=0; i<p->num_slides; i++ ) {
		free_slide(p->slides[i]);
	}

	/* FIXME: Loads of stuff leaks here */
	free(p->filename);
	imagestore_destroy(p->is);
	free(p);

	if ( final ) {
		gtk_main_quit();
	}
}


/* "pos" is the index that the caller would like this slide to have */
int insert_slide(struct presentation *p, struct slide *new, int pos)
{
	struct slide **try;
	int i;

	try = realloc(p->slides, (1+p->num_slides)*sizeof(struct slide *));
	if ( try == NULL ) {
		free(new);
		return 1;
	}
	p->slides = try;
	p->completely_empty = 0;

	if ( p->num_slides > 0 ) {
		int j = pos;
		if ( j == 0 ) j = 1;
		for ( i=p->num_slides; i>=j; i-- ) {
			p->slides[i] = p->slides[i-1];
		}
	}

	p->slides[pos] = new;
	p->num_slides++;

	new->parent = p;

	return 0;
}


static void delete_slide_index(struct presentation *p, int pos)
{
	int i;

	for ( i=pos; i<p->num_slides-1; i++ ) {
		p->slides[i] = p->slides[i+1];
	}

	p->num_slides--;

	/* Don't bother to resize the array */
}


void delete_slide(struct presentation *p, struct slide *s)
{
	int idx;

	idx = slide_number(p, s);
	delete_slide_index(p, idx);

	free_slide(s);
}


struct slide *new_slide()
{
	struct slide *new;

	new = calloc(1, sizeof(struct slide));
	if ( new == NULL ) return NULL;

	new->scblocks = NULL;
	new->notes = NULL;

	return new;
}


void free_slide(struct slide *s)
{
	free(s);
}


struct slide *add_slide(struct presentation *p, int pos)
{
	struct slide *s = new_slide();
	if ( insert_slide(p, s, pos) ) {
		free_slide(s);
		return NULL;
	}

	return s;
}


static char *safe_basename(const char *in)
{
	int i;
	char *cpy;
	char *res;

	cpy = strdup(in);

	/* Get rid of any trailing slashes */
	for ( i=strlen(cpy)-1; i>0; i-- ) {
		if ( cpy[i] == '/' ) {
			cpy[i] = '\0';
		} else {
			break;
		}
	}

	/* Find the base name */
	for ( i=strlen(cpy)-1; i>0; i-- ) {
		if ( cpy[i] == '/' ) {
			i++;
			break;
		}
	}

	res = strdup(cpy+i);
	/* If we didn't find a previous slash, i==0 so res==cpy */

	free(cpy);

	return res;
}


char *get_titlebar_string(struct presentation *p)
{
	if ( p->filename == NULL ) {
		return strdup("(untitled)");
	} else {
		return safe_basename(p->filename);
	}
}


int slide_number(struct presentation *p, struct slide *s)
{
	int i;

	for ( i=0; i<p->num_slides; i++ ) {
		if ( p->slides[i] == s ) return i;
	}

	return p->num_slides;
}


struct presentation *new_presentation()
{
	struct presentation *new;

	new = calloc(1, sizeof(struct presentation));
	if ( new == NULL ) return NULL;

	new->filename = NULL;
	new->titlebar = get_titlebar_string(new);

	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	new->num_slides = 0;
	new->slides = NULL;
	add_slide(new, 0);

	new->completely_empty = 1;
	new->stylesheet = NULL;
	new->is = imagestore_new();

	return new;
}


int save_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	char *old_fn;

	// FIXME grab_current_notes(p);

	fh = fopen(filename, "w");
	if ( fh == NULL ) return 1;

	save_sc_block(fh, p->scblocks);

	/* Slightly fiddly because someone might
	 * do save_presentation(p, p->filename) */
	old_fn = p->filename;
	imagestore_set_presentation_file(p->is, filename);
	p->filename = strdup(filename);
	if ( old_fn != NULL ) free(old_fn);
	update_titlebar(p);

	fclose(fh);
	return 0;
}


static char *fgets_long(FILE *fh, size_t *lp)
{
	char *line;
	size_t la;
	size_t l = 0;

	la = 1024;
	line = malloc(la);
	if ( line == NULL ) return NULL;

	do {

		int r;

		r = fgetc(fh);
		if ( r == EOF ) {
			free(line);
			*lp = 0;
			return NULL;
		}

		line[l++] = r;

		if ( r == '\n' ) {
			line[l++] = '\0';
			*lp = l;
			return line;
		}

		if ( l == la ) {

			char *ln;

			la += 1024;
			ln = realloc(line, la);
			if ( ln == NULL ) {
				free(line);
				*lp = 0;
				return NULL;
			}

			line = ln;

		}

	} while ( 1 );
}


int load_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	int r = 0;
	char *everything;
	size_t el = 1;
	SCBlock *block;

	everything = strdup("");

	assert(p->completely_empty);
	delete_slide(p, p->slides[0]);

	fh = fopen(filename, "r");
	if ( fh == NULL ) return 1;

	while ( !feof(fh) ) {

		size_t len = 0;
		char *line = fgets_long(fh, &len);

		if ( line != NULL ) {

			everything = realloc(everything, el+len);
			if ( everything == NULL ) {
				r = 1;
				break;
			}
			el += len;

			strcat(everything, line);
		}

	}

	fclose(fh);

	p->scblocks = sc_parse(everything);
	free(everything);

	if ( p->scblocks == NULL ) r = 1;

	if ( r ) {
		p->completely_empty = 1;
		fprintf(stderr, "Parse error.\n");
		return r;  /* Error */
	}

	find_stylesheet(p);

	block = p->scblocks;
	while ( block != NULL ) {

		const char *n = sc_block_name(block);

		if ( n == NULL ) goto next;

		if ( strcmp(n, "slide") == 0 ) {

			struct slide *s = add_slide(p, p->num_slides);

			if ( s != NULL ) {

				s->scblocks = sc_block_child(block);
				attach_notes(s);

			}

		}

next:
		block = sc_block_next(block);

	}

	assert(p->filename == NULL);
	p->filename = strdup(filename);
	update_titlebar(p);
	imagestore_set_presentation_file(p->is, filename);

	return 0;
}

