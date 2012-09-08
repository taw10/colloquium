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
#include "stylesheet.h"
#include "mainwindow.h"


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


void show_tree(struct ds_node *root, const char *path)
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


struct ds_node *find_node(struct ds_node *root, const char *path, int cr)
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

			if ( cr ) {
				cur = add_child(cur, element);
				if ( cur == NULL ) {
					return NULL;  /* Error */
				}
			} else {
				return NULL;
			}

		}

	}

out:
	return cur;
}


static void parse_line(struct ds_node *root, struct ds_node **cn,
                       const char *line)
{
	size_t i;
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
	struct ds_node *cur_node = *cn;

	len = strlen(line);

	s_start = len;

	for ( i=0; i<len; i++ ) {
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
		return;
	}

	if ( !h_openbracket && !h_equals ) return;

	if ( !h_openbracket && (!h_start || !h_val || !h_equals) ) {
		fprintf(stderr, "Incomplete assignment: %s", line);
		return;
	}

	if ( h_equals && (h_openbracket || h_closebracket) ) {
		fprintf(stderr, "Brackets and equals: %s", line);
		return;
	}

	if ( !h_openbracket ) {

		size_t pos = 0;
		char *key;
		char *value;
		struct ds_node *node;

		key = malloc(len);
		value = malloc(len);

		for ( i=s_start; i<s_equals; i++ ) {
			if ( !isspace(line[i]) ) key[pos++] = line[i];
		}
		key[pos] = '\0';

		pos = 0;
		for ( i=s_val; i<len; i++ ) {
			if ( line[i] != '\n' ) value[pos++] = line[i];
		}
		value[pos] = '\0';

		node = find_node(cur_node, key, 1);
		node->value = strdup(value);

		free(key);
		free(value);

	} else {

		size_t pos = 0;
		char *path;

		path = malloc(len);

		for ( i=s_openbracket+1; i<s_closebracket; i++ ) {
			if ( !isspace(line[i]) ) path[pos++] = line[i];
		}
		path[pos] = '\0';
		cur_node = find_node(root, path, 1);

		free(path);

	}

	*cn = cur_node;
}


static char *fgets_long(FILE *fh)
{
	char *line;
	size_t la, l;

	la = 1024;
	line = malloc(la);
	if ( line == NULL ) return NULL;

	l = 0;
	do {

		int r;

		r = fgetc(fh);
		if ( r == EOF ) {
			free(line);
			return NULL;
		}

		if ( r == '\n' ) {
			line[l++] = '\0';
			return line;
		}

		line[l++] = r;

		if ( l == la ) {

			char *ln;

			la += 1024;
			ln = realloc(line, la);
			if ( ln == NULL ) {
				free(line);
				return NULL;
			}

			line = ln;

		}

	} while ( 1 );
}


static int deserialize_file(FILE *fh, struct ds_node *root)
{
	char *line;
	struct ds_node *cur_node = root;

	line = NULL;
	do {

		line = fgets_long(fh);
		if ( line == NULL ) {
			if ( ferror(fh) ) printf("Read error!\n");
			continue;
		}

		parse_line(root, &cur_node, line);

	} while ( line != NULL );

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


char *escape_text(const char *a)
{
	char *b;
	size_t l1, l, i;

	l1 = strlen(a);

	b = malloc(2*l1 + 1);
	l = 0;

	for ( i=0; i<l1; i++ ) {

		char c = a[i];

		/* Yes, this is horribly confusing */
		if ( c == '\n' ) {
			b[l++] = '\\';  b[l++] = 'n';
		} else if ( c == '\r' ) {
			b[l++] = '\\';  b[l++] = 'r';
		} else if ( c == '\"' ) {
			b[l++] = '\\';  b[l++] = '\"';
		} else if ( c == '\t' ) {
			b[l++] = '\\';  b[l++] = 't';
		} else {
			b[l++] = c;
		}

	}
	b[l++] = '\0';

	return realloc(b, l);
}


char *unescape_text(const char *a)
{
	char *b;
	size_t l1, l, i;
	int escape;

	l1 = strlen(a);

	b = malloc(l1 + 1);
	l = 0;
	escape = 0;

	for ( i=0; i<l1; i++ ) {

		char c = a[i];

		if ( escape ) {
			if ( c == 'r' ) b[l++] = '\r';
			if ( c == 'n' ) b[l++] = '\n';
			if ( c == '\"' ) b[l++] = '\"';
			if ( c == 't' ) b[l++] = '\t';
			escape = 0;
			continue;
		}

		if ( c == '\\' ) {
			escape = 1;
			continue;
		}

		b[l++] = c;

	}
	b[l++] = '\0';

	return realloc(b, l);
}



int get_field_f(struct ds_node *root, const char *key, double *val)
{
	struct ds_node *node;
	double v;
	char *check;

	node = find_node(root, key, 0);
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


int get_field_i(struct ds_node *root, const char *key, int *val)
{
	struct ds_node *node;
	int v;
	char *check;

	node = find_node(root, key, 0);
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


int get_field_s(struct ds_node *root, const char *key, char **val)
{
	struct ds_node *node;
	char *v;
	size_t i, len, s1, s2;
	int hq;

	node = find_node(root, key, 0);
	if ( node == NULL ) {
		*val = NULL;
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
		fprintf(stderr, "Mismatched quotes in '%s'\n", node->value);
		return 1;
	}

	v = malloc(s2-s1+1);
	if ( v == NULL ) {
		fprintf(stderr, "Failed to allocate space for '%s'\n", key);
		return 1;
	}

	strncpy(v, node->value+s1+1, s2-s1-1);
	v[s2-s1-1] = '\0';

	*val = unescape_text(v);
	free(v);

	return 0;
}


static struct slide *tree_to_slide(struct presentation *p, struct ds_node *root)
{
	struct slide *s;

	s = new_slide();
	s->parent = p;

	/* FIXME: Load stuff */

	return s;
}


static int tree_to_slides(struct ds_node *root, struct presentation *p)
{
	int i;

	for ( i=0; i<root->n_children; i++ ) {

		struct slide *s;

		s = tree_to_slide(p, root->children[i]);
		if ( s != NULL ) {
			insert_slide(p, s, p->num_slides-1);
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

	if ( r ) {
		p->cur_edit_slide = new_slide();
		insert_slide(p, p->cur_edit_slide, 0);
		p->completely_empty = 1;
		return r;  /* Error */
	}

	assert(p->filename == NULL);
	p->filename = strdup(filename);
	update_titlebar(p);

	p->cur_edit_slide = p->slides[0];

	return 0;
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
	char *n;

	n = escape_text(val);
	if ( n == NULL ) {
		fprintf(stderr, "Failed to escape '%s'\n", val);
		return;
	}

	check_prefix_output(ser);
	fprintf(ser->fh, "%s = \"%s\"\n", key, n);

	free(n);
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
	ser->empty_set = 1;
}


int save_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	int i;
	struct serializer ser;
	char *old_fn;

	//grab_current_notes(p);

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

		s = p->slides[i];

		snprintf(s_id, 31, "%i", i);
		serialize_start(&ser, s_id);

		/* FIXME: Save stuff */

		serialize_end(&ser);

	}
	serialize_end(&ser);

	/* Slightly fiddly because someone might
	 * do save_presentation(p, p->filename) */
	old_fn = p->filename;
	p->filename = strdup(filename);
	if ( old_fn != NULL ) free(old_fn);
	update_titlebar(p);

	fclose(fh);
	return 0;
}