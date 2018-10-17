/*
 * stylesheet.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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

#include <json-glib/json-glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "stylesheet.h"


struct _stylesheet {
	JsonNode *root;
};


Stylesheet *stylesheet_load(const char *filename)
{
	JsonParser *parser;
	gboolean r;
	GError *err = NULL;
	Stylesheet *ss;

	ss = calloc(1, sizeof(Stylesheet));
	if ( ss == NULL ) return NULL;

	parser = json_parser_new();

	r = json_parser_load_from_file(parser, filename, &err);
	if ( r == FALSE ) {
		fprintf(stderr, "Failed to load style sheet: '%s'\n", err->message);
		return NULL;
	}

	ss->root = json_parser_steal_root(parser);
	g_object_unref(parser);

	return ss;
}


char *stylesheet_lookup(Stylesheet *ss, const char *path)
{
	JsonNode *node;
	JsonArray *array;
	GError *err = NULL;
	char *ret;
	const gchar *v;

	node = json_path_query(path, ss->root, &err);
	array = json_node_get_array(node);

	v = json_array_get_string_element(array, 0);
	if ( v == NULL ) return NULL;

	ret = strdup(v);
	json_node_unref(node);
	return ret;
}


void stylesheet_free(Stylesheet *ss)
{
	g_object_unref(ss->root);
	free(ss);
}

