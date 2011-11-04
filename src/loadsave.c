/*
 * loadsave.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "presentation.h"
#include "objects.h"
#include "stylesheet.h"


struct ds_node
{
	char *key;
	char *value;
	struct ds_node **children;
	int n_children;
	int max_children;
};


static int alloc_children(struct ds_node *node)
{
	struct ds_node **new;

	new = realloc(node->children,
	              node->max_children*sizeof(*node->children));
	if ( new == NULL ) return 1;

	node->children = new;
	return 0;
}


static struct ds_node *new_ds_node(const char *key)
{
	struct ds_node *new;

	new = malloc(sizeof(*new));
	if ( new == NULL ) return NULL;

	new->key = strdup(key);
	if ( new->key == NULL ) {
		free(new);
		return NULL;
	}

	new->value = NULL;
	new->n_children = 0;
	new->max_children = 32;
	new->children = NULL;

	if ( alloc_children(new) ) {
		free(new);
		return NULL;
	}

	return new;
}


static struct ds_node *add_child(struct ds_node *node, const char *key)
{
	struct ds_node *new;

	new = new_ds_node(key);
	if ( new == NULL ) return NULL;

	if ( node->n_children >= new->max_children ) {
		new->max_children += 32;
		if ( alloc_children(node) ) {
			free(new);
			return NULL;
		}
	}

	node->children[node->n_children++] = new;

	return new;
}


static void UNUSED show_tree(struct ds_node *root, const char *path)
{
	char newpath[1024];
	int i;

	snprintf(newpath, 1023, "%s%s/", path, root->key);

	printf("%s\n", newpath);
	for ( i=0; i<root->n_children; i++ ) {
		printf("     %s => %s\n", root->children[i]->key,
		                          root->children[i]->value);
	}

	for ( i=0; i<root->n_children; i++ ) {
		if ( root->children[i]->n_children > 0 ) {
			printf("\n");
			show_tree(root->children[i], newpath);
		}
	}
}


static struct ds_node *find_node(struct ds_node *root, const char *path)
{
	size_t start, len;
	char element[1024];
	struct ds_node *cur = root;

	len = strlen(path);

	start = 0;
	while ( start < len ) {

		size_t pos, i;
		int child;
		int found = 0;

		pos = 0;
		for ( i=start; i<len; i++ ) {

			if ( path[i] == '/' ) break;
			element[pos++] = path[i];

		}
		element[pos++] = '\0';
		if ( element[0] == '\0' ) {
			goto out;
		}
		start = i+1;

		for ( child=0; child<cur->n_children; child++ ) {

			const char *this_key = cur->children[child]->key;

			if ( strcmp(this_key, element) == 0 ) {
				cur = cur->children[child];
				found = 1;
				break;
			}

		}

		if ( !found ) {

			cur = add_child(cur, element);
			if ( cur == NULL ) {
				return NULL;  /* Error */
			}

		}

	}

out:
	return cur;
}


static int deserialize_file(FILE *fh, struct ds_node *root)
{
	char *rval = NULL;
	struct ds_node *cur_node = root;

	do {
		size_t i;
		char line[1024];
		size_t len, s_start;
		size_t s_equals = 0;
		size_t s_val = 0;
		size_t s_openbracket = 0;
		size_t s_closebracket = 0;
		int h_start = 0;
		int h_equals = 0;
		int h_val = 0;
		int h_openbracket = 0;
		int h_closebracket = 0;

		rval = fgets(line, 1023, fh);
		if ( rval == NULL ) {
			if ( ferror(fh) ) printf("Read error!\n");
			continue;
		}

		len = strlen(line);
		s_start = len-1;

		for ( i=0; i<len-1; i++ ) {
			if ( !h_start && !isspace(line[i]) ) {
				s_start = i;
				h_start = 1;
			}
			if ( !h_val && h_equals && !isspace(line[i]) ) {
				s_val = i;
				h_val = 1;
			}
			if ( !h_equals && (line[i] == '=') ) {
				s_equals = i;
				h_equals = 1;
			}
			if ( !h_openbracket && (line[i] == '[') ) {
				s_openbracket = i;
				h_openbracket = 1;
			}
			if ( h_openbracket && !h_closebracket
			     && (line[i] == ']') )
			{
				s_closebracket = i;
				h_closebracket = 1;
			}
		}

		if ( (h_openbracket && !h_closebracket)
		  || (!h_openbracket && h_closebracket) )
		{
			fprintf(stderr, "Mismatched square brackets: %s", line);
			continue;
		}

		if ( !h_openbracket && !h_equals ) continue;

		if ( !h_openbracket && (!h_start || !h_val || !h_equals) ) {
			fprintf(stderr, "Incomplete assignment: %s", line);
			continue;
		}

		if ( h_equals && (h_openbracket || h_closebracket) ) {
			fprintf(stderr, "Brackets and equals: %s", line);
			continue;
		}

		if ( !h_openbracket ) {

			size_t pos = 0;
			char key[1024];
			char value[1024];
			struct ds_node *node;

			for ( i=s_start; i<s_equals; i++ ) {
				if ( !isspace(line[i]) ) key[pos++] = line[i];
			}
			key[pos] = '\0';

			pos = 0;
			for ( i=s_val; i<len; i++ ) {
				if ( line[i] != '\n' ) value[pos++] = line[i];
			}
			value[pos] = '\0';

			node = find_node(cur_node, key);
			node->value = strdup(value);

		} else {

			size_t pos = 0;
			char path[1024];

			for ( i=s_openbracket+1; i<s_closebracket; i++ ) {
				if ( !isspace(line[i]) ) path[pos++] = line[i];
			}
			path[pos] = '\0';
			cur_node = find_node(root, path);

		}

	} while ( rval != NULL );

	return 0;
}


static void free_ds_tree(struct ds_node *root)
{
	int i;

	for ( i=0; i<root->n_children; i++ ) {
		if ( root->children[i]->n_children > 0 ) {
			free_ds_tree(root->children[i]);
		}
	}

	free(root->key);
	free(root->value);  /* Might free(NULL), but that's fine */
	free(root);
}


static int get_field_f(struct ds_node *root, const char *key, double *val)
{
	struct ds_node *node;
	double v;
	char *check;

	node = find_node(root, key);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find field '%s'\n", key);
		return 1;
	}

	v = strtod(node->value, &check);
	if ( check == node->value ) {
		fprintf(stderr, "Invalid value for '%s'\n", key);
		return 1;
	}

	*val = v;

	return 0;
}


static int get_field_i(struct ds_node *root, const char *key, int *val)
{
	struct ds_node *node;
	int v;
	char *check;

	node = find_node(root, key);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find field '%s'\n", key);
		return 1;
	}

	v = strtol(node->value, &check, 0);
	if ( check == node->value ) {
		fprintf(stderr, "Invalid value for '%s'\n", key);
		return 1;
	}

	*val = v;

	return 0;
}


static int get_field_s(struct ds_node *root, const char *key, char **val)
{
	struct ds_node *node;
	char *v;
	size_t i, len, s1, s2;
	int hq;

	node = find_node(root, key);
	if ( node == NULL ) {
		fprintf(stderr, "Couldn't find field '%s'\n", key);
		return 1;
	}

	len = strlen(node->value);
	hq = 0;
	for ( i=0; i<len; i++ ) {
		if ( node->value[i] == '"' ) {
			s1 = i;
			hq = 1;
			break;
		}
	}
	if ( !hq ) {
		fprintf(stderr, "No quotes in '%s'\n", node->value);
		return 1;
	}

	for ( i=len-1; i>=0; i-- ) {
		if ( node->value[i] == '"' ) {
			s2 = i;
			break;
		}
	}

	if ( s1 == s2 ) {
		fprintf(stderr, "Mismatchd quotes in '%s'\n", node->value);
		return 1;
	}

	v = malloc(s2-s1+1);
	if ( v == NULL ) {
		fprintf(stderr, "Failed to allocate space for '%s'\n", key);
		return 1;
	}

	strncpy(v, node->value+s1+1, s2-s1-1);
	v[s2-s1-1] = '\0';

	*val = v;

	return 0;
}


static int read_style(struct style *sty, struct ds_node *root)
{
	char *align;

	get_field_f(root, "margin_left",   &sty->margin_left);
	get_field_f(root, "margin_right",  &sty->margin_right);
	get_field_f(root, "margin_top",    &sty->margin_top);
	get_field_f(root, "margin_bottom", &sty->margin_bottom);

	get_field_i(root, "use_max_width", &sty->use_max_width);
	get_field_f(root, "max_width",     &sty->max_width);

	get_field_f(root, "offset_x",      &sty->offset_x);
	get_field_f(root, "offset_y",      &sty->offset_y);

	get_field_s(root, "font",          &sty->font);
	get_field_s(root, "colour",        &sty->colour);
	get_field_f(root, "alpha",         &sty->alpha);

	get_field_s(root, "halign",        &align);
	sty->halign = str_to_halign(align);
	free(align);
	get_field_s(root, "valign",        &align);
	sty->valign = str_to_valign(align);
	free(align);

	return 0;
}


static StyleSheet *tree_to_stylesheet(struct ds_node *root)
{
	StyleSheet *ss;
	struct ds_node *node;
	int i;

	ss = new_stylesheet();
	if ( ss == NULL ) return NULL;

	node = find_node(root, "styles");
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

	return ss;
}


int tree_to_presentation(struct ds_node *root, struct presentation *p)
{
	struct ds_node *node;
	char *check;

	node = find_node(root, "slide-properties/width");
	if ( node == NULL ) return 1;
	p->slide_width = strtod(node->value, &check);
	if ( check == node->value ) {
		fprintf(stderr, "Invalid slide width\n");
		return 1;
	}

	node = find_node(root, "slide-properties/height");
	if ( node == NULL ) return 1;
	p->slide_height = strtod(node->value, &check);
	if ( check == node->value ) {
		fprintf(stderr, "Invalid slide height\n");
		return 1;
	}

	node = find_node(root, "stylesheet");
	if ( node != NULL ) {
		free_stylesheet(p->ss);
		p->ss = tree_to_stylesheet(node);
		if ( p->ss == NULL ) {
			fprintf(stderr, "Invalid style sheet\n");
			return 1;
		}
	}

	return 0;
}


int load_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	struct ds_node *root;
	int r;

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

	if ( r ) return r;  /* Error */

	p->cur_edit_slide = p->slides[0];

	return 0;
}


static const char *type_text(enum objtype t)
{
	switch ( t )
	{
		case TEXT : return "text";
		default : return "unknown";
	}
}


static void rebuild_prefix(struct serializer *ser)
{
	int i;
	size_t sz = 1;  /* Space for terminator */

	for ( i=0; i<ser->stack_depth; i++ ) {
		sz += strlen(ser->stack[i]) + 1;
	}

	free(ser->prefix);
	ser->prefix = malloc(sz);
	if ( ser->prefix == NULL ) return;  /* Probably bad! */

	ser->prefix[0] = '\0';
	for ( i=0; i<ser->stack_depth; i++ ) {
		if ( i != 0 ) strcat(ser->prefix, "/");
		strcat(ser->prefix, ser->stack[i]);
	}
}


void serialize_start(struct serializer *ser, const char *id)
{
	ser->stack[ser->stack_depth++] = strdup(id);
	rebuild_prefix(ser);
	ser->empty_set = 1;
}


static void check_prefix_output(struct serializer *ser)
{
	if ( ser->empty_set ) {
		ser->empty_set = 0;
		if ( ser->prefix != NULL ) {
			fprintf(ser->fh, "\n");
			fprintf(ser->fh, "[%s]\n", ser->prefix);
		}
	}
}


void serialize_s(struct serializer *ser, const char *key, const char *val)
{
	check_prefix_output(ser);
	fprintf(ser->fh, "%s = \"%s\"\n", key, val);
}


void serialize_f(struct serializer *ser, const char *key, double val)
{
	check_prefix_output(ser);
	fprintf(ser->fh, "%s = %.2f\n", key, val);
}


void serialize_b(struct serializer *ser, const char *key, int val)
{
	check_prefix_output(ser);
	fprintf(ser->fh, "%s = %i\n", key, val);
}


void serialize_end(struct serializer *ser)
{
	free(ser->stack[--ser->stack_depth]);
	rebuild_prefix(ser);
}


int save_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	int i;
	struct serializer ser;

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

		int j;
		struct slide *s;
		char s_id[32];

		s = p->slides[i];

		snprintf(s_id, 31, "%i", i);
		serialize_start(&ser, s_id);
		for ( j=0; j<s->num_objects; j++ ) {

			struct object *o = s->objects[j];
			char o_id[32];

			if ( o->empty ) continue;
			snprintf(o_id, 31, "%i", j);

			serialize_start(&ser, o_id);
			serialize_s(&ser, "type", type_text(o->type));
			o->serialize(o, &ser);
			serialize_end(&ser);

		}
		serialize_end(&ser);

	}
	serialize_end(&ser);

	fclose(fh);
	return 0;
}
