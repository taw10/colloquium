/*
 * presentation.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
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
#include <gtk/gtk.h>

#include "presentation.h"
#include "stylesheet.h"


static int num_presentations = 0;


void free_presentation(struct presentation *p)
{
	int i;
	int final = 0;

	for ( i=0; i<p->num_slides; i++ ) {
		free_slide(p->slides[i]);
	}

	(*p->num_presentations)--;
	if ( *p->num_presentations == 0 ) final = 1;

	/* FIXME: Loads of stuff leaks here */
	free(p->filename);
	free(p);

	if ( final ) {
		gtk_main_quit();
	}
}


int insert_slide(struct presentation *p, struct slide *new, int pos)
{
	struct slide **try;
	
	try = realloc(p->slides, (1+p->num_slides)*sizeof(struct slide *));
	if ( try == NULL ) {
		free(new);
		return 1;
	}
	p->slides = try;
	p->completely_empty = 0;

	/* Insert into list.  Yuk yuk yuk etc. */
	if ( (p->num_slides>1) && (pos<p->num_slides-1) ) {

		int i;

		for ( i=p->num_slides; i>pos+1; i-- ) {
			p->slides[i] = p->slides[i-1];
		}
		p->slides[pos+1] = new;

	} else if ( pos == p->num_slides-1 ) {

		p->slides[pos+1] = new;

	} else {
		assert(pos == 0);
		p->slides[pos] = new;
	}

	new->parent = p;
	p->num_slides++;

	return 0;
}


struct slide *new_slide()
{
	struct slide *new;

	new = calloc(1, sizeof(struct slide));
	if ( new == NULL ) return NULL;

	new->rendered_edit = NULL;
	new->rendered_proj = NULL;
	new->rendered_thumb = NULL;

	new->notes = strdup("");

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


void get_titlebar_string(struct presentation *p)
{
	free(p->titlebar);

	if ( p->filename == NULL ) {
		p->titlebar = strdup("(untitled)");
	} else {
		p->titlebar = safe_basename(p->filename);
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

	num_presentations++;
	new->num_presentations = &num_presentations;

	new->filename = NULL;
	new->titlebar = NULL;
	get_titlebar_string(new);

	new->window = NULL;
	new->ui = NULL;
	new->action_group = NULL;
	new->slideshow = NULL;
	new->notes = NULL;

	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	/* Add one blank slide and view it */
	new->num_slides = 0;
	new->slides = NULL;
	new->cur_edit_slide = NULL;
	new->cur_proj_slide = NULL;

	new->completely_empty = 1;

	//new->ss = new_stylesheet();
	//default_stylesheet(new->ss);

	return new;
}
