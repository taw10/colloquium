/*
 * stylesheet.c
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
#include <gtk/gtk.h>
#include <assert.h>

#include "presentation.h"
#include "stylesheet.h"
#include "loadsave.h"


struct _stylesheet
{
	struct style          **styles;
	int                     n_styles;

	struct slide_template **templates;
	int                     n_templates;
};


struct style *new_style(StyleSheet *ss, const char *name)
{
	struct style *sty;
	int n;
	struct style **styles_new;

	sty = calloc(1, sizeof(*sty));
	if ( sty == NULL ) return NULL;

	sty->name = strdup(name);

	/* DS9K compatibility */
	sty->lop.x = 0.0;
	sty->lop.y = 0.0;
	sty->lop.w = 100.0;
	sty->lop.h = 100.0;

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
		free(ss->styles[i]->sc_prologue);
		free(ss->styles[i]);
	}

	free(ss->styles);
	free(ss);
}


extern const char *_binary_src_default_stylesheet_sty_start;
extern const char *_binary_src_default_stylesheet_sty_size;

StyleSheet *default_stylesheet()
{
	char *v;
	struct ds_node *root;
	StyleSheet *ss;
	size_t len;

	len = (size_t)&_binary_src_default_stylesheet_sty_size;
	v = malloc(len+1);
	if ( v == NULL ) return NULL;

	memcpy(v, &_binary_src_default_stylesheet_sty_start, len);
	v[len] = '\0';

	root = new_ds_node("root");
	deserialize_memory(v, root);
	ss = tree_to_stylesheet(root);
	free(v);
	return ss;
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


static const char *units(LengthUnits un)
{
	switch ( un ) {
		case UNITS_SLIDE : return "u";
		case UNITS_FRAC : return "fr";
	}
	return "annoyingly unspecified units";
}


static void serialize_f_units(struct serializer *s, const char *key, double val,
                              LengthUnits un)
{
	char tmp[64];

	snprintf(tmp, 63, "%.2f %s", val, units(un));
	serialize_s(s, key, tmp);
}


static int style_number(StyleSheet *ss, struct style *s)
{
	int i;

	for ( i=0; i<ss->n_styles; i++ ) {
		if ( ss->styles[i] == s ) return i;
	}

	return -1;
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
		serialize_f(ser, "pad_l", s->lop.pad_l);
		serialize_f(ser, "pad_r", s->lop.pad_r);
		serialize_f(ser, "pad_t", s->lop.pad_t);
		serialize_f(ser, "pad_b", s->lop.pad_b);
		serialize_f_units(ser, "w", s->lop.w, s->lop.w_units);
		serialize_f_units(ser, "h", s->lop.h, s->lop.h_units);
		serialize_s(ser, "prologue", s->sc_prologue);
		serialize_end(ser);

	}
	serialize_end(ser);

	serialize_start(ser, "templates");
	for ( i=0; i<ss->n_templates; i++ ) {

		struct slide_template *t = ss->templates[i];
		char id[32];
		int j;

		snprintf(id, 31, "%i", i);

		serialize_start(ser, id);
		serialize_s(ser, "name", t->name);
		for ( j=0; j<t->n_styles; j++ ) {

			struct style *s = t->styles[j];
			char id[32];

			snprintf(id, 31, "sty%i", i);

			serialize_i(ser, id, style_number(ss, s));

		}
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


struct slide_template *new_template(StyleSheet *ss, const char *name)
{
	struct slide_template *t;
	int n;
	struct slide_template **templates_new;

	t = calloc(1, sizeof(*t));
	if ( t == NULL ) return NULL;

	t->name = strdup(name);

	n = ss->n_templates;
	templates_new = realloc(ss->templates, (n+1)*sizeof(t));
	if ( templates_new == NULL ) {
		free(t->name);
		free(t);
		return NULL;
	}
	ss->templates = templates_new;
	ss->templates[n] = t;
	ss->n_templates = n+1;

	return t;
}


void add_to_template(struct slide_template *t, struct style *sty)
{
	int n;
	struct style **styles_new;

	n = t->n_styles;
	styles_new = realloc(t->styles, (n+1)*sizeof(sty));
	if ( styles_new == NULL ) {
		fprintf(stderr, "Failed to add style '%s' to template '%s'.\n",
		        sty->name, t->name);
		return;
	}
	t->styles = styles_new;
	t->styles[n] = sty;
	t->n_styles = n+1;
}


struct _styleiterator
{
	int n;
};

struct style *style_first(StyleSheet *ss, StyleIterator **piter)
{
	StyleIterator *iter;

	if ( ss->n_styles == 0 ) return NULL;

	iter = calloc(1, sizeof(StyleIterator));
	if ( iter == NULL ) return NULL;

	iter->n = 0;
	*piter = iter;

	return ss->styles[0];
}


struct style *style_next(StyleSheet *ss, StyleIterator *iter)
{
	iter->n++;
	if ( iter->n == ss->n_styles ) {
		free(iter);
		return NULL;
	}

	return ss->styles[iter->n];
}


struct _templateiterator
{
	int n;
};

struct slide_template *template_first(StyleSheet *ss, TemplateIterator **piter)
{
	TemplateIterator *iter;

	if ( ss->n_templates == 0 ) return NULL;

	iter = calloc(1, sizeof(TemplateIterator));
	if ( iter == NULL ) return NULL;

	iter->n = 0;
	*piter = iter;

	return ss->templates[0];
}


struct slide_template *template_next(StyleSheet *ss, TemplateIterator *iter)
{
	iter->n++;
	if ( iter->n == ss->n_templates ) {
		free(iter);
		return NULL;
	}

	return ss->templates[iter->n];
}
