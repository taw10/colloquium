/*
 * loadsave.h
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

#ifndef LOADSAVE_H
#define LOADSAVE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Forward declaration */
struct presentation;

struct ds_node
{
	char *key;
	char *value;
	struct ds_node **children;
	int n_children;
	int max_children;
};

/* Would be opaque if I could be bothered to write the constructor */
struct serializer
{
	FILE *fh;

	char *stack[32];
	int stack_depth;
	char *prefix;
	int empty_set;
	int blank_written;
};

extern void serialize_start(struct serializer *s, const char *id);
extern void serialize_s(struct serializer *s, const char *key, const char *val);
extern void serialize_f(struct serializer *s, const char *key, double val);
extern void serialize_b(struct serializer *s, const char *key, int val);
extern void serialize_end(struct serializer *s);

extern int get_field_f(struct ds_node *root, const char *key, double *val);
extern int get_field_i(struct ds_node *root, const char *key, int *val);
extern int get_field_s(struct ds_node *root, const char *key, char **val);

extern struct ds_node *find_node(struct ds_node *root, const char *path);

extern int load_presentation(struct presentation *p, const char *filename);
extern int save_presentation(struct presentation *p, const char *filename);

#endif	/* LOADSAVE_H */
