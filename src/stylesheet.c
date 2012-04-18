/*
 * stylesheet.c
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

#include "presentation.h"
#include "stylesheet.h"
#include "objects.h"
#include "loadsave.h"


struct _stylesheet
{
	struct slide_template **slide_templates;
	int                     n_slide_templates;
};


struct slide_template *new_slide_template(StyleSheet *ss, const char *name)
{
	struct slide_template *st;
	int n;
	struct slide_template **slide_templates_new;

	st = calloc(1, sizeof(*st));
	if ( st == NULL ) return NULL;

	st->name = strdup(name);
	st->frame_classes = NULL;
	st->n_frame_classes = 0;
	st->bgblocks = NULL;
	st->n_bgblocks = 0;

	n = ss->n_slide_templates;

	/* Resize to n_slide_templates * size of pointer */
	slide_templates_new = realloc(ss->slide_templates, (n+1)*sizeof(st));
	if ( slide_templates_new == NULL ) {
		free(st->name);
		free(st);
		return NULL;
	}
	ss->slide_templates = slide_templates_new;
	ss->slide_templates[n] = st;
	ss->n_slide_templates = n+1;

	return st;
}


struct frame_class *new_frame_class(struct slide_template *st, const char *name)
{
	struct frame_class *fc;
	int n;
	struct frame_class **frame_classes_new;

	fc = calloc(1, sizeof(*fc));
	if ( fc == NULL ) return NULL;

	fc->name = strdup(name);
	fc->sc_prologue = NULL;

	n = st->n_frame_classes;
	frame_classes_new = realloc(st->frame_classes, (n+1)*sizeof(fc));
	if ( frame_classes_new == NULL ) {
		free(fc->name);
		free(fc);
		return NULL;
	}
	st->frame_classes = frame_classes_new;
	st->frame_classes[n] = fc;
	st->n_frame_classes = n+1;

	return fc;
}


static void free_frame_class(struct frame_class *fc)
{
	free(fc->name);
	free(fc->sc_prologue);
}


static void free_bgblock(struct bgblock *bg)
{
	free(bg->colour1);
	free(bg->colour2);
}


static void free_slide_template(struct slide_template *st)
{
	int i;

	for ( i=0; i<st->n_frame_classes; i++ ) {
		free_frame_class(st->frame_classes[i]);
	}
	free(st->frame_classes);

	for ( i=0; i<st->n_bgblocks; i++ ) {
		free_bgblock(st->bgblocks[i]);
	}
	free(st->bgblocks);

	free(st->name);
}


void free_stylesheet(StyleSheet *ss)
{
	int i;

	for ( i=0; i<ss->n_slide_templates; i++ ) {
		free_slide_template(ss->slide_templates[i]);
	}

	free(ss->slide_templates);
	free(ss);
}


void default_stylesheet(StyleSheet *ss)
{
	struct slide_template *st;
	struct frame_class *fc;

	st = new_slide_template(ss, "Title page");

	fc = new_frame_class(st, "Presentation title");
	fc->margin_left = 20.0;
	fc->margin_right = 20.0;
	fc->margin_top = 20.0;
	fc->margin_bottom = 20.0;
	fc->sc_prologue = strdup("\\FF'Sans 50';\\FC'#000000000000';");

	fc = new_frame_class(st, "Author(s)");
	fc->margin_left = 20.0;
	fc->margin_right = 20.0;
	fc->margin_top = 20.0;
	fc->margin_bottom = 20.0;
	fc->sc_prologue = strdup("\\FF'Sans 30';\\FC'#000000000000';");

	fc = new_frame_class(st, "Date");
	fc->margin_left = 20.0;
	fc->margin_right = 20.0;
	fc->margin_top = 20.0;
	fc->margin_bottom = 20.0;
	fc->sc_prologue = strdup("\\FF'Sans 30';\\FC'#000000000000';");

	st->bgblocks = malloc(sizeof(struct bgblock));
	st->n_bgblocks = 1;
	st->bgblocks[0]->type = BGBLOCK_SOLID;
	st->bgblocks[0]->min_x = 0.0;
	st->bgblocks[0]->max_x = 1024.0;
	st->bgblocks[0]->min_y = 0.0;
	st->bgblocks[0]->max_y = 768.0;
	st->bgblocks[0]->colour1 = strdup("#eeeeeeeeeeee");
	st->bgblocks[0]->alpha1 = 1.0;

	st = new_slide_template(ss, "Slide");

	fc = new_frame_class(st, "Content");
	fc->margin_left = 20.0;
	fc->margin_right = 20.0;
	fc->margin_top = 20.0;
	fc->margin_bottom = 20.0;
	fc->sc_prologue = strdup("\\FF'Sans 18';\\FC'#000000000000';");

	fc = new_frame_class(st, "Title");
	fc->margin_left = 20.0;
	fc->margin_right = 20.0;
	fc->margin_top = 20.0;
	fc->margin_bottom = 20.0;
	fc->sc_prologue = strdup("\\FF'Sans 40';\\FC'#000000000000';");

	fc = new_frame_class(st, "Credit");
	fc->margin_left = 20.0;
	fc->margin_right = 20.0;
	fc->margin_top = 20.0;
	fc->margin_bottom = 35.0;
	fc->sc_prologue = strdup("\\FF'Sans 14';\\FC'#000000000000';");

	fc = new_frame_class(st, "Date");
	fc->margin_left = 600.0;
	fc->margin_right = 100.0;
	fc->margin_top = 745.0;
	fc->margin_bottom = 5.0;
	fc->sc_prologue = strdup("\\FF'Sans 12';\\FC'#999999999999';");

	fc = new_frame_class(st, "Slide number");
	fc->margin_left = 600.0;
	fc->margin_right = 5.0;
	fc->margin_top = 745.0;
	fc->margin_bottom = 5.0;
	fc->sc_prologue = strdup("\\FF'Sans 12';\\FC'#999999999999';\\JR");

	fc = new_frame_class(st, "Presentation title");
	fc->margin_left = 5.0;
	fc->margin_right = 600.0;
	fc->margin_top = 745.0;
	fc->margin_bottom = 5.0;
	fc->sc_prologue = strdup("\\FF'Sans 12';\\FC'#999999999999';\\JC\\VC");

	st->bgblocks = malloc(sizeof(struct bgblock));
	st->n_bgblocks = 1;
	st->bgblocks[0]->type = BGBLOCK_SOLID;
	st->bgblocks[0]->min_x = 0.0;
	st->bgblocks[0]->max_x = 1024.0;
	st->bgblocks[0]->min_y = 0.0;
	st->bgblocks[0]->max_y = 768.0;
	st->bgblocks[0]->colour1 = strdup("#ffffffffffff");
	st->bgblocks[0]->alpha1 = 1.0;
}


static const char *str_bgtype(enum bgblocktype t)
{
	switch ( t ) {
		case BGBLOCK_SOLID             : return "solid";
		case BGBLOCK_GRADIENT_X        : return "gradient_x";
		case BGBLOCK_GRADIENT_Y        : return "gradient_y";
		case BGBLOCK_GRADIENT_CIRCULAR : return "gradient_circular";
		case BGBLOCK_IMAGE             : return "image";
		default : return "???";
	}
}


static enum bgblocktype str_to_bgtype(char *t)
{
	if ( strcmp(t, "solid") == 0 ) return BGBLOCK_SOLID;
	if ( strcmp(t, "gradient_x") == 0 ) return BGBLOCK_GRADIENT_X;
	if ( strcmp(t, "gradient_y") == 0 ) return BGBLOCK_GRADIENT_Y;
	if ( strcmp(t, "gradient_ciruclar") == 0 ) {
		return BGBLOCK_GRADIENT_CIRCULAR;
	}
	if ( strcmp(t, "image") == 0 ) return BGBLOCK_IMAGE;

	return BGBLOCK_SOLID;
}


static int read_frame_class(struct frame_class *fc, struct ds_node *root)
{
	get_field_f(root, "margin_left",   &fc->margin_left);
	get_field_f(root, "margin_right",  &fc->margin_right);
	get_field_f(root, "margin_top",    &fc->margin_top);
	get_field_f(root, "margin_bottom", &fc->margin_bottom);
	get_field_s(root, "sc_prologue",   &fc->sc_prologue);

	return 0;
}


static int read_bgblock(struct bgblock *b, struct ds_node *root)
{
	char *type;

	get_field_s(root, "type",  &type);
	b->type = str_to_bgtype(type);

	get_field_f(root, "min_x",  &b->min_x);
	get_field_f(root, "max_x",  &b->max_x);
	get_field_f(root, "min_y",  &b->min_y);
	get_field_f(root, "max_y",  &b->max_y);

	switch ( b->type ) {

		case BGBLOCK_SOLID :
		get_field_s(root, "colour1",  &b->colour1);
		get_field_f(root, "alpha1",  &b->alpha1);
		break;

		case BGBLOCK_GRADIENT_X :
		case BGBLOCK_GRADIENT_Y :
		case BGBLOCK_GRADIENT_CIRCULAR :
		get_field_s(root, "colour1",  &b->colour1);
		get_field_f(root, "alpha1",  &b->alpha1);
		get_field_s(root, "colour2",  &b->colour2);
		get_field_f(root, "alpha2",  &b->alpha2);
		break;

		default:
		break;

	}

	return 0;
}


struct slide_template *tree_to_slide_template(StyleSheet *ss,
                                              struct ds_node *root)
{
	struct slide_template *st;
	int i;
	char *v;
	struct ds_node *node;

	get_field_s(root, "name", &v);
	if ( v == NULL ) {
		fprintf(stderr, "No name for slide template '%s'\n",
		        root->children[i]->key);
		return NULL;
	}

	st = new_slide_template(ss, v);
	if ( st == NULL ) return NULL;

	node = find_node(root, "frame_classes", 0);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find frame classes\n");
		free_slide_template(st);
		return NULL;
	}

	st->frame_classes = malloc(node->n_children
	                           * sizeof(struct frame_class));
	if ( st->frame_classes == NULL ) {
		fprintf(stderr, "Couldn't allocate frame classes\n");
		free_slide_template(st);
		return NULL;
	}
	st->n_frame_classes = node->n_children;

	for ( i=0; i<node->n_children; i++ ) {

		struct frame_class *fc;

		fc = st->frame_classes[i];

		if ( read_frame_class(fc, node->children[i]) ) {
			fprintf(stderr, "Couldn't read frame class %i\n", i);
			continue;
		}

	}

	node = find_node(root, "bgblocks", 0);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find bgblocks\n");
		free_stylesheet(ss);
		return NULL;
	}

	st->bgblocks = malloc(node->n_children * sizeof(struct bgblock));
	if ( st->bgblocks == NULL ) {
		fprintf(stderr, "Couldn't allocate bgblocks\n");
		free_slide_template(st);
		return NULL;
	}
	st->n_bgblocks = node->n_children;

	for ( i=0; i<node->n_children; i++ ) {

		struct bgblock *b;

		b = st->bgblocks[i];

		if ( read_bgblock(b, node->children[i]) ) {
			fprintf(stderr, "Couldn't read bgblock %i\n", i);
			continue;
		}

	}

	return st;
}


StyleSheet *tree_to_stylesheet(struct ds_node *root)
{
	StyleSheet *ss;
	struct ds_node *node;
	int i;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

	node = find_node(root, "templates", 0);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find slide templates\n");
		free_stylesheet(ss);
		return NULL;
	}

	for ( i=0; i<node->n_children; i++ ) {

		struct slide_template *st;

		st = tree_to_slide_template(ss, node->children[i]);

	}

	return ss;
}


StyleSheet *new_stylesheet()
{
	StyleSheet *ss;

	ss = calloc(1, sizeof(struct _stylesheet));
	if ( ss == NULL ) return NULL;

	ss->n_slide_templates = 0;
	ss->slide_templates = NULL;

	return ss;
}


int save_stylesheet(StyleSheet *ss, const char *filename)
{
	FILE *fh;
	struct serializer ser;

	fh = fopen(filename, "w");
	if ( fh == NULL ) return 1;

	/* Set up the serializer */
	ser.fh = fh;
	ser.stack_depth = 0;
	ser.prefix = NULL;

	fprintf(fh, "# Colloquium style sheet file\n");
	serialize_f(&ser, "version", 0.1);

	serialize_start(&ser, "stylesheet");
	write_stylesheet(ss, &ser);
	serialize_end(&ser);

	return 0;
}


StyleSheet *load_stylesheet(const char *filename)
{
	StyleSheet *ss;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

	/* FIXME: Implement this */

	return ss;
}


void write_stylesheet(StyleSheet *ss, struct serializer *ser)
{
	int i;

	serialize_start(ser, "templates");
	for ( i=0; i<ss->n_slide_templates; i++ ) {

		int j;
		struct slide_template *st;

		st = ss->slide_templates[i];

		serialize_start(ser, "bgblocks");
		for ( j=0; j<st->n_bgblocks; j++ ) {

			struct bgblock *b = st->bgblocks[j];
			char id[32];

			snprintf(id, 31, "%i", j);

			serialize_start(ser, id);
			serialize_s(ser, "type", str_bgtype(b->type));
			serialize_f(ser, "min_x", b->min_x);
			serialize_f(ser, "min_y", b->min_y);
			serialize_f(ser, "max_x", b->max_x);
			serialize_f(ser, "max_y", b->max_y);

			switch ( b->type ) {

				case BGBLOCK_SOLID :
				serialize_s(ser, "colour1", b->colour1);
				serialize_f(ser, "alpha1", b->alpha1);
				break;

				case BGBLOCK_GRADIENT_X :
				case BGBLOCK_GRADIENT_Y :
				case BGBLOCK_GRADIENT_CIRCULAR :
				serialize_s(ser, "colour1", b->colour1);
				serialize_f(ser, "alpha1", b->alpha1);
				serialize_s(ser, "colour2", b->colour2);
				serialize_f(ser, "alpha2", b->alpha2);
				break;

				default:
				break;

			}

			serialize_end(ser);

		}
		serialize_end(ser);

		serialize_start(ser, "frame_classes");
		for ( j=0; j<st->n_frame_classes; j++ ) {

			struct frame_class *fc = st->frame_classes[j];
			char id[32];

			snprintf(id, 31, "%i", j);

			serialize_start(ser, id);
			serialize_s(ser, "name", fc->name);
			serialize_f(ser, "margin_left", fc->margin_left);
			serialize_f(ser, "margin_right", fc->margin_right);
			serialize_f(ser, "margin_top", fc->margin_top);
			serialize_f(ser, "margin_bottom", fc->margin_bottom);
			serialize_s(ser, "sc_prologue", fc->sc_prologue);
			serialize_end(ser);

		}
		serialize_end(ser);

	}

	serialize_end(ser);
}


struct frame_class *find_frame_class(struct slide_template *st,
                                     const char *name)
{
	int i;
	for ( i=0; i<st->n_frame_classes; i++ ) {
		if ( strcmp(st->frame_classes[i]->name, name) == 0 ) {
			return st->frame_classes[i];
		}
	}

	return NULL;
}


struct slide_template *find_slide_template(StyleSheet *ss, const char *name)
{
	int i;
	for ( i=0; i<ss->n_slide_templates; i++ ) {
		if ( strcmp(ss->slide_templates[i]->name, name) == 0 ) {
			return ss->slide_templates[i];
		}
	}

	return NULL;
}
