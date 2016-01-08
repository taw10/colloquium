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
	int final = 0;

	/* FIXME: Loads of stuff leaks here */
	free(p->filename);
	imagestore_destroy(p->is);
	free(p);

	if ( final ) {
		gtk_main_quit();
	}
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


struct presentation *new_presentation()
{
	struct presentation *new;

	new = calloc(1, sizeof(struct presentation));
	if ( new == NULL ) return NULL;

	new->filename = NULL;
	new->titlebar = get_titlebar_string(new);

	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	new->completely_empty = 1;
	new->stylesheet = NULL;
	new->is = imagestore_new();

	/* FIXME: Hardcoded */
	new->lang = pango_language_from_string("en_GB");

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


static int safe_strcmp(const char *a, const char *b)
{
	if ( a == NULL ) return 1;
	if ( b == NULL ) return 1;
	return strcmp(a, b);
}


int slide_number(struct presentation *p, SCBlock *sl)
{
	SCBlock *bl = p->scblocks;
	int n = 0;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			n++;
			if ( sc_block_child(bl) == sl ) return n;
		}
		bl = sc_block_next(bl);
	}

	return 0;
}


int num_slides(struct presentation *p)
{
	SCBlock *bl = p->scblocks;
	int n = 0;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			n++;
		}
		bl = sc_block_next(bl);
	}

	return n;
}


/* Warning: isn't very clever and assumes the block we want is at the top
 * level */
static SCBlock *parent_block(struct presentation *p, SCBlock *findme)
{
	SCBlock *bl;

	bl = p->scblocks;

	while ( bl != NULL ) {
		if ( sc_block_child(bl) == findme ) return bl;
		bl = sc_block_next(bl);
	}

	printf("Whoops, couldn't find parent!\n");
	return bl;
}


SCBlock *first_slide(struct presentation *p)
{
	SCBlock *bl = p->scblocks;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			return sc_block_child(bl);
		}
		bl = sc_block_next(bl);
	}

	fprintf(stderr, "Couldn't find first slide!\n");
	return NULL;
}


SCBlock *last_slide(struct presentation *p)
{
	SCBlock *bl = p->scblocks;
	SCBlock *l = NULL;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			l = bl;
		}
		bl = sc_block_next(bl);
	}

	if ( l == NULL ) {
		fprintf(stderr, "Couldn't find last slide!\n");
	}
	return sc_block_child(l);
}


SCBlock *next_slide(struct presentation *p, SCBlock *sl)
{
	SCBlock *pp = parent_block(p, sl);
	SCBlock *bl = pp;
	int found = 0;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			if ( found ) return sc_block_child(bl);
		}
		if ( bl == pp ) {
			found = 1;
		}
		bl = sc_block_next(bl);
	}

	fprintf(stderr, "Couldn't find next slide!\n");
	return NULL;
}


SCBlock *prev_slide(struct presentation *p, SCBlock *sl)
{
	SCBlock *pp = parent_block(p, sl);
	SCBlock *bl = p->scblocks;
	SCBlock *l = NULL;

	while ( bl != NULL ) {
		if ( bl == pp ) {
			if ( l == NULL ) return sl; /* Already on first slide */
			return sc_block_child(l);
		}
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			l = bl;
		}
		bl = sc_block_next(bl);
	}

	fprintf(stderr, "Couldn't find prev slide!\n");
	return NULL;
}


int load_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	int r = 0;
	char *everything;
	size_t el = 1;

	everything = strdup("");

	assert(p->completely_empty);

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

	/* FIXME: Hardcoded */
	p->lang = pango_language_from_string("en_GB");

	if ( p->scblocks == NULL ) r = 1;

	if ( r ) {
		p->completely_empty = 1;
		fprintf(stderr, "Parse error.\n");
		return r;  /* Error */
	}

	find_stylesheet(p);

	assert(p->filename == NULL);
	p->filename = strdup(filename);
	update_titlebar(p);
	imagestore_set_presentation_file(p->is, filename);

	return 0;
}

