/*
 * presentation.c
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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
#include "stylesheet.h"
#include "loadsave.h"
#include "mainwindow.h"
#include "frame.h"
#include "imagestore.h"
#include "wrap.h"
#include "notes.h"
#include "inhibit_screensaver.h"
#include "render.h"


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

	if ( p->inhibit != NULL ) inhibit_cleanup(p->inhibit);

	/* FIXME: Loads of stuff leaks here */
	free(p->filename);
	imagestore_destroy(p->is);
	free(p);

	if ( final ) {
		gtk_main_quit();
	}
}


static void renumber_slides(struct presentation *p)
{
	int i;
	for ( i=0; i<p->num_slides; i++ ) {
		if ( p->slides[i]->constants != NULL ) {
			p->slides[i]->constants->slide_number = i+1;
		} else {
			fprintf(stderr, "Slide %i has no constants.\n", i);
		}
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
	p->num_slides++;

	if ( p->num_slides > 0 ) {
		for ( i=p->num_slides-1; i>=pos; i-- ) {
			p->slides[i] = p->slides[i-1];
		}
	}

	p->slides[pos] = new;

	new->parent = p;

	renumber_slides(p);

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

	renumber_slides(p);
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

	new->rendered_edit = NULL;
	new->rendered_proj = NULL;
	new->rendered_thumb = NULL;

	new->constants = calloc(1, sizeof(struct slide_constants));
	if ( new->constants == NULL ) {
		free(new);
		return NULL;
	}

	new->top = frame_new();
	/* FIXME: Set zero margins etc on top level frame */

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


static int alloc_selection(struct presentation *p)
{
	struct frame **new_selection;
	new_selection = realloc(p->selection,
	                        p->max_selection*sizeof(struct frame *));
	if ( new_selection == NULL ) return 1;
	p->selection = new_selection;
	return 0;
}


struct presentation *new_presentation()
{
	struct presentation *new;

	new = calloc(1, sizeof(struct presentation));
	if ( new == NULL ) return NULL;

	new->constants = calloc(1, sizeof(struct presentation_constants));
	if ( new->constants == NULL ) {
		free(new);
		return NULL;
	}

	num_presentations++;
	new->num_presentations = &num_presentations;

	new->filename = NULL;
	new->titlebar = NULL;
	get_titlebar_string(new);

	new->window = NULL;
	new->ui = NULL;
	new->action_group = NULL;
	new->slideshow = NULL;

	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	new->edit_slide_width = 1024;
	new->proj_slide_width = 2048;
	new->thumb_slide_width = 180;

	/* Add one blank slide and view it */
	new->num_slides = 0;
	new->slides = NULL;
	new->cur_edit_slide = NULL;
	new->cur_proj_slide = NULL;

	new->completely_empty = 1;

	new->ss = default_stylesheet();

	new->n_menu_rebuild = 0;
	new->menu_rebuild_list = NULL;
	new->menu_path_list = NULL;

	new->selection = NULL;
	new->n_selection = 0;
	new->max_selection = 64;
	if ( alloc_selection(new) ) return NULL;

	new->is = imagestore_new();

	return new;
}


static char *maybe_star(int i)
{
	if ( i ) {
		return "*";
	} else {
		return "";
	}
}


static char *frame_options_string(struct frame *fr, StyleSheet *ss)
{
	char *opt;

	opt = malloc(64);
	if ( opt == NULL ) return NULL;

	snprintf(opt, 31, "style=%s%s",
	         fr->style->pname, maybe_star(fr->lop_from_style));

	if ( !fr->lop_from_style ) {
		char tmp[32];
		snprintf(tmp, 31, ",%.1f%sx%.1f%s+%.1f+%.1f",
			         fr->lop.w, units(fr->lop.w_units),
				 fr->lop.h, units(fr->lop.h_units),
				 fr->lop.x, fr->lop.y);
		strcat(opt, tmp);
	}

	return opt;
}


char *packed_sc(struct frame *fr, StyleSheet *ss)
{
	char *sc;
	int i;
	size_t len = 0;

	if ( fr->sc != NULL ) {
		len += strlen(fr->sc)+1;
		sc = malloc(len);
		memcpy(sc, fr->sc, len);
	} else {
		len = 0;
		sc = malloc(1);
		sc[0] = '\0';
	}

	for ( i=0; i<fr->num_children; i++ ) {

		char *ch_sc;
		char *scn;
		size_t ch_len;
		char *frame_opts;

		ch_sc = packed_sc(fr->children[i], ss);
		ch_len = strlen(ch_sc);

		frame_opts = frame_options_string(fr->children[i], ss);

		len += ch_len + 64;
		scn = malloc(len + ch_len);
		snprintf(scn, len, "%s\\f[%s]{%s}", sc, frame_opts, ch_sc);
		free(ch_sc);
		free(sc);
		sc = scn;

	}

	return sc;
}


int save_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	int i;
	struct serializer ser;
	char *old_fn;

	grab_current_notes(p);

	fh = fopen(filename, "w");
	if ( fh == NULL ) return 1;

	/* Set up the serializer */
	ser.fh = fh;
	ser.stack_depth = 0;
	ser.prefix = NULL;

	fprintf(fh, "# Colloquium presentation file\n");
	serialize_f(&ser, "version", 0.1);

	serialize_start(&ser, "slide-properties");
	serialize_f(&ser, "width", p->slide_width);
	serialize_f(&ser, "height", p->slide_height);
	serialize_end(&ser);

	serialize_start(&ser, "stylesheet");
	write_stylesheet(p->ss, &ser);
	serialize_end(&ser);

	serialize_start(&ser, "slides");
	for ( i=0; i<p->num_slides; i++ ) {

		struct slide *s;
		char s_id[32];
		char *sc;

		s = p->slides[i];

		snprintf(s_id, 31, "%i", i);
		serialize_start(&ser, s_id);

		sc = packed_sc(s->top, p->ss);
		serialize_s(&ser, "sc", sc);
		free(sc);

		write_notes(s, &ser);

		serialize_end(&ser);

	}
	serialize_end(&ser);

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


static struct slide *tree_to_slide(struct presentation *p, struct ds_node *root)
{
	struct slide *s;
	char *sc;

	s = new_slide();
	s->parent = p;

	load_notes(root, s);

	get_field_s(root, "sc", &sc);
	s->top = sc_unpack(sc, p->ss);
	free(sc);

	return s;
}


static int tree_to_slides(struct ds_node *root, struct presentation *p)
{
	int i;

	for ( i=0; i<root->n_children; i++ ) {

		struct slide *s;

		s = tree_to_slide(p, root->children[i]);
		if ( s != NULL ) {
			insert_slide(p, s, p->num_slides);
		}

	}

	return 0;
}


int tree_to_presentation(struct ds_node *root, struct presentation *p)
{
	struct ds_node *node;
	char *check;
	int i;

	p->cur_edit_slide = NULL;
	p->cur_proj_slide = NULL;

	node = find_node(root, "slide-properties/width", 0);
	if ( node == NULL ) return 1;
	p->slide_width = strtod(node->value, &check);
	if ( check == node->value ) {
		fprintf(stderr, "Invalid slide width\n");
		return 1;
	}

	node = find_node(root, "slide-properties/height", 0);
	if ( node == NULL ) return 1;
	p->slide_height = strtod(node->value, &check);
	if ( check == node->value ) {
		fprintf(stderr, "Invalid slide height\n");
		return 1;
	}

	node = find_node(root, "stylesheet", 0);
	if ( node != NULL ) {
		free_stylesheet(p->ss);
		p->ss = tree_to_stylesheet(node);
		if ( p->ss == NULL ) {
			fprintf(stderr, "Invalid style sheet\n");
			return 1;
		}
	}

	for ( i=0; i<p->num_slides; i++ ) {
		free_slide(p->slides[i]);
		p->num_slides = 0;
	}

	node = find_node(root, "slides", 0);
	if ( node != NULL ) {
		tree_to_slides(node, p);
		if ( p->num_slides == 0 ) {
			fprintf(stderr, "Failed to load any slides\n");
			p->cur_edit_slide = add_slide(p, 0);
			return 1;
		}
	}

	return 0;
}


int load_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	struct ds_node *root;
	int r, i;

	assert(p->completely_empty);

	fh = fopen(filename, "r");
	if ( fh == NULL ) return 1;

	root = new_ds_node("root");
	if ( root == NULL ) return 1;

	if ( deserialize_file(fh, root) ) {
		fclose(fh);
		return 1;
	}

	r = tree_to_presentation(root, p);
	free_ds_tree(root);

	fclose(fh);

	if ( r ) {
		p->cur_edit_slide = new_slide();
		insert_slide(p, p->cur_edit_slide, 0);
		p->completely_empty = 1;
		return r;  /* Error */
	}

	assert(p->filename == NULL);
	p->filename = strdup(filename);
	update_titlebar(p);
	imagestore_set_presentation_file(p->is, filename);

	p->cur_edit_slide = p->slides[0];

	for ( i=0; i<p->num_slides; i++ ) {
		struct slide *s = p->slides[i];
		s->rendered_thumb = render_slide(s, p->thumb_slide_width,
	                                         p->slide_width,
		                                 p->slide_height,
		                                 p->is,  ISZ_THUMBNAIL);
	}

	return 0;
}


static struct frame *find_parent(struct frame *fr, struct frame *search)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {
		if ( fr->children[i] == search ) {
			return fr;
		}
	}

	for ( i=0; i<fr->num_children; i++ ) {
		struct frame *tt;
		tt = find_parent(fr->children[i], search);
		if ( tt != NULL ) return tt;
	}

	return NULL;
}


void delete_subframe(struct slide *s, struct frame *fr)
{
	struct frame *parent;
	int i, idx, found;

	parent = find_parent(s->top, fr);
	if ( parent == NULL ) {
		fprintf(stderr, "Couldn't find parent when deleting frame.\n");
		return;
	}


	found = 0;
	for ( i=0; i<parent->num_children; i++ ) {
		if ( parent->children[i] == fr ) {
			idx = i;
			found = 1;
			break;
		}
	}

	if ( !found ) {
		fprintf(stderr, "Couldn't find child when deleting frame.\n");
		return;
	}

	for ( i=idx; i<parent->num_children-1; i++ ) {
		parent->children[i] = parent->children[i+1];
	}

	parent->num_children--;
}


void set_edit(struct presentation *p, struct slide *s)
{
	p->cur_edit_slide = s;
}


void set_selection(struct presentation *p, struct frame *fr)
{
	if ( p->n_selection != 0 ) {
		int i;
		for ( i=0; i<p->n_selection; i++ ) {
			if ( p->selection[i]->empty ) {
				delete_subframe(p->cur_edit_slide,
				                p->selection[i]);
			}
		}
	}

	p->selection[0] = fr;
	p->n_selection = 1;

	if ( fr == NULL ) p->n_selection = 0;
}


void add_selection(struct presentation *p, struct frame *fr)
{
	if ( p->n_selection == p->max_selection ) {
		p->max_selection += 64;
		if ( alloc_selection(p) ) {
			fprintf(stderr, "Not enough memory for selection.\n");
			return;
		}
	}

	p->selection[p->n_selection++] = fr;
}
