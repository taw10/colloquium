/*
 * presentation.c
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
#include <gtk/gtk.h>

#include "presentation.h"
#include "objects.h"
#include "stylesheet.h"
#include "tool_select.h"
#include "tool_text.h"
#include "tool_image.h"


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
//	int i;

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

	/* Update slide numbers for all subsequent slides */
//	for ( i=pos+1; i<p->num_slides; i++ ) {
//		struct object *o = p->slides[i]->roles[S_ROLE_SLIDENUMBER];
//		if ( o != NULL ) {
//			o->update_object(o);
//		}
//	}

	return 0;
}


struct slide *new_slide()
{
	struct slide *new;

	new = calloc(1, sizeof(struct slide));
	if ( new == NULL ) return NULL;

	/* No objects to start with */
	new->num_objects = 0;
	new->objects = NULL;

	new->rendered_edit = NULL;
	new->rendered_proj = NULL;
	new->rendered_thumb = NULL;

	new->notes = strdup("");

	return new;
}


void free_slide(struct slide *s)
{
	int i;

	for ( i=0; i<s->num_objects; i++ ) {
		delete_object(s->objects[i]);
	}

	free(s);
}


struct slide *add_slide(struct presentation *p, int pos)
{
	struct slide *s = new_slide();
	if ( insert_slide(p, s, pos) ) {
		free_slide(s);
		return NULL;
	}

#if 0
	/* Copy roles and references to this slide as applicable */
	if ( pos >= 0 ) {

		struct slide *ex = p->slides[pos];

		s->roles[S_ROLE_PTITLE_REF] = ex->roles[S_ROLE_PTITLE_REF];
		s->roles[S_ROLE_PAUTHOR_REF] = ex->roles[S_ROLE_PAUTHOR_REF];
		s->roles[S_ROLE_PDATE_REF] = ex->roles[S_ROLE_PDATE_REF];

		if ( ex->roles[S_ROLE_PTITLE] != NULL ) {
			p->text_tool->create_default(p,
			             ex->roles[S_ROLE_PTITLE]->style, s,
			             p->text_tool);
		}

		if ( ex->roles[S_ROLE_SLIDENUMBER] != NULL ) {
			p->text_tool->create_default(p,
			             ex->roles[S_ROLE_SLIDENUMBER]->style, s,
			             p->text_tool);
		}

		if ( ex->roles[S_ROLE_PAUTHOR] != NULL ) {
			p->text_tool->create_default(p,
			             ex->roles[S_ROLE_PAUTHOR]->style, s,
			             p->text_tool);
		}

		if ( ex->roles[S_ROLE_PDATE] != NULL ) {
			p->text_tool->create_default(p,
			             ex->roles[S_ROLE_PDATE]->style, s,
			             p->text_tool);
		}

	}
#endif

	return s;
}


int add_object_to_slide(struct slide *s, struct object *o)
{
	struct object **try;

	try = realloc(s->objects, (1+s->num_objects)*sizeof(struct object *));
	if ( try == NULL ) return 1;
	s->objects = try;

	s->objects[s->num_objects++] = o;
	o->parent = s;

	s->parent->completely_empty = 0;

	return 0;
}


void remove_object_from_slide(struct slide *s, struct object *o)
{
	int i;
	int found = 0;

	for ( i=0; i<s->num_objects; i++ ) {

		if ( s->objects[i] == o ) {
			assert(!found);
			found = 1;
		}

		if ( found ) {
			if ( i == s->num_objects-1 ) {
				s->objects[i] = NULL;
			} else {
				s->objects[i] = s->objects[i+1];
			}
		}

	}

	s->num_objects--;
}


struct object *find_object_at_position(struct slide *s, double x, double y)
{
	int i;
	struct object *o = NULL;

	for ( i=0; i<s->num_objects; i++ ) {

		if ( (x>s->objects[i]->x) && (y>s->objects[i]->y)
		  && (x<s->objects[i]->x+s->objects[i]->bb_width)
		  && (y<s->objects[i]->y+s->objects[i]->bb_height) )
		{
			o = s->objects[i];
		}

	}

	return o;
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

	/* FIXME: Should be just one of these */
	new->prefs = calloc(1, sizeof(struct prefs));
	new->prefs->b_splits = 1;
	new->prefs->open_notes = 0;

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

	new->cur_frame = NULL;
	new->completely_empty = 1;
	new->drag_status = DRAG_STATUS_NONE;

	new->ss = new_stylesheet();
	default_stylesheet(new->ss);
	new->image_store = image_store_new();

	return new;
}
