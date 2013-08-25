/*
 * loadsave.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "presentation.h"
#include "stylesheet.h"


static int alloc_children(struct ds_node *node)
{
	struct ds_node **new;

	new = realloc(node->children,
	              node->max_children*sizeof(*node->children));
	if ( new == NULL ) return 1;

	node->children = new;
	return 0;
}


struct ds_node *new_ds_node(const char *key)
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
		if ( !h_openbracket && (line[i] == '[') && !h_equals ) {
			s_openbracket = i;
			h_openbracket = 1;
		}
		if ( h_openbracket && !h_closebracket
		     && (line[i] == ']')  && !h_equals )
		{
			s_closebracket = i;
			h_closebracket = 1;
		}
	}

	if ( (h_openbracket && !h_closebracket)
	  || (!h_openbracket && h_closebracket) )
	{
		fprintf(stderr, "Mismatched square brackets: %s\n", line);
		return;
	}

	if ( !h_openbracket && !h_equals ) return;

	if ( !h_openbracket && (!h_start || !h_val || !h_equals) ) {
		fprintf(stderr, "Incomplete assignment: %s\n", line);
		return;
	}

	if ( h_equals && (h_openbracket || h_closebracket) ) {
		fprintf(stderr, "Brackets and equals: %s\n", line);
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


int deserialize_file(FILE *fh, struct ds_node *root)
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


int deserialize_memory(const char *s, struct ds_node *root)
{
	char *end;
	int done;
	struct ds_node *cur_node = root;

	done = 0;
	do {

		char *line;

		end = strchr(s, '\n');
		if ( end == NULL ) {
			parse_line(root, &cur_node, s);
			done = 1;
		} else {
			line = strndup(s, end-s);
			parse_line(root, &cur_node, line);
			free(line);
			s = end+1;
		}

	} while ( !done );

	return 0;
}


void free_ds_tree(struct ds_node *root)
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
			else if ( c == 'n' ) b[l++] = '\n';
			else if ( c == '\"' ) b[l++] = '\"';
			else if ( c == 't' ) b[l++] = '\t';
			else {
				b[l++] = '\\';
				b[l++] = c;
			}
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
		v = strdup(node->value);  /* Use the whole thing */
	} else {

		for ( i=len-1; i>=0; i-- ) {
			if ( node->value[i] == '"' ) {
				s2 = i;
				break;
			}
		}

		if ( s1 == s2 ) {
			fprintf(stderr, "Mismatched quotes in '%s'\n",
			        node->value);
			return 1;
		}

		v = malloc(s2-s1+1);
		if ( v == NULL ) {
			fprintf(stderr, "Failed to allocate space for '%s'\n",
			        key);
			return 1;
		}

		strncpy(v, node->value+s1+1, s2-s1-1);
		v[s2-s1-1] = '\0';
	}

	*val = unescape_text(v);
	free(v);

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


void serialize_i(struct serializer *ser, const char *key, int val)
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

