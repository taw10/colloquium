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
#include <gtk/gtk.h>
#include <assert.h>

#include "presentation.h"
#include "stylesheet.h"
#include "loadsave.h"


struct style *new_style(StyleSheet *ss, const char *name)
{
	struct style *sty;
	int n;
	struct style **styles_new;

	sty = calloc(1, sizeof(*sty));
	if ( sty == NULL ) return NULL;

	sty->name = strdup(name);

	n = ss->n_styles;
	styles_new = realloc(ss->styles, (n+1)*sizeof(sty));
	if ( styles_new == NULL ) {
		free(sty->name);
		free(sty);
		return NULL;
	}
	ss->styles = styles_new;
	ss->styles[n] = sty;
	ss->n_styles = n+1;

	return sty;
}


void free_stylesheet(StyleSheet *ss)
{
	int i;

	for ( i=0; i<ss->n_styles; i++ ) {
		free(ss->styles[i]->name);
		free(ss->styles[i]);
	}

	free(ss->styles);
	free(ss);
}


void default_stylesheet(StyleSheet *ss)
{
	struct style *sty;

	/* Default style must be first */
	sty = new_style(ss, "Default");
	sty->lop.margin_l = 20.0;
	sty->lop.margin_r = 20.0;
	sty->lop.margin_t = 20.0;
	sty->lop.margin_b = 20.0;

	sty = new_style(ss, "Slide title");
	sty->lop.margin_l = 20.0;
	sty->lop.margin_r = 20.0;
	sty->lop.margin_t = 20.0;
	sty->lop.margin_b = 20.0;
}


static int read_style(struct style *sty, struct ds_node *root)
{
	get_field_f(root, "margin_l", &sty->lop.margin_l);
	get_field_f(root, "margin_r", &sty->lop.margin_r);
	get_field_f(root, "margin_t", &sty->lop.margin_t);
	get_field_f(root, "margin_b", &sty->lop.margin_b);

	return 0;
}


StyleSheet *tree_to_stylesheet(struct ds_node *root)
{
	StyleSheet *ss;
	struct ds_node *node;
	int i;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

	node = find_node(root, "styles", 0);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find styles\n");
		free_stylesheet(ss);
		return NULL;
	}

	for ( i=0; i<node->n_children; i++ ) {

		struct style *ns;
		char *v;

		get_field_s(node->children[i], "name", &v);
		if ( v == NULL ) {
			fprintf(stderr, "No name for style '%s'\n",
			        node->children[i]->key);
			continue;
		}

		ns = new_style(ss, v);
		if ( ns == NULL ) {
			fprintf(stderr, "Couldn't create style for '%s'\n",
			        node->children[i]->key);
			continue;
		}

		if ( read_style(ns, node->children[i]) ) {
			fprintf(stderr, "Couldn't read style '%s'\n", v);
			continue;
		}

	}

	node = find_node(root, "bgblocks", 0);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find bgblocks\n");
		free_stylesheet(ss);
		return NULL;
	}

	return ss;
}


StyleSheet *new_stylesheet()
{
	StyleSheet *ss;

	ss = calloc(1, sizeof(struct _stylesheet));
	if ( ss == NULL ) return NULL;

	ss->n_styles = 0;
	ss->styles = NULL;

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

	serialize_start(ser, "styles");
	for ( i=0; i<ss->n_styles; i++ ) {

		struct style *s = ss->styles[i];
		char id[32];

		snprintf(id, 31, "%i", i);

		serialize_start(ser, id);
		serialize_s(ser, "name", s->name);
		serialize_f(ser, "margin_l", s->lop.margin_l);
		serialize_f(ser, "margin_r", s->lop.margin_r);
		serialize_f(ser, "margin_t", s->lop.margin_t);
		serialize_f(ser, "margin_b", s->lop.margin_b);
		serialize_end(ser);

	}
	serialize_end(ser);
}


struct style *find_style(StyleSheet *ss, const char *name)
{
	int i;
	for ( i=0; i<ss->n_styles; i++ ) {
		if ( strcmp(ss->styles[i]->name, name) == 0 ) {
			return ss->styles[i];
		}
	}

	return NULL;
}
