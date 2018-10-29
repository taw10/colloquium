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
#include <gio/gio.h>
#include <gdk/gdk.h>

#include "stylesheet.h"
#include "utils.h"


struct _stylesheet {
	JsonNode *root;
};


static int find_comma(const char *a)
{
	int i = 0;
	int in_brackets = 0;
	size_t len = strlen(a);

	do {
		if ( (a[i] == ',') && !in_brackets ) return i;
		if ( a[i] == '(' ) in_brackets++;
		if ( a[i] == ')' ) in_brackets--;
		i++;
	} while ( i < len );
	return 0;
}


int parse_colour_duo(const char *a, GdkRGBA *col1, GdkRGBA *col2)
{
	char *acopy;
	int cpos;

	acopy = strdup(a);
	if ( acopy == NULL ) return 1;

	cpos = find_comma(acopy);
	if ( cpos == 0 ) {
		fprintf(stderr, _("Invalid bg gradient spec '%s'\n"), a);
		return 1;
	}

	acopy[cpos] = '\0';

	if ( gdk_rgba_parse(col1, acopy) != TRUE ) {
		fprintf(stderr, _("Failed to parse colour: %s\n"), acopy);
	}
	if ( gdk_rgba_parse(col2, &acopy[cpos+1]) != TRUE ) {
		fprintf(stderr, _("Failed to parse colour: %s\n"), &acopy[cpos+1]);
	}

	free(acopy);
	return 0;
}


Stylesheet *stylesheet_load(GFile *file)
{
	JsonParser *parser;
	gboolean r;
	GError *err = NULL;
	Stylesheet *ss;
	char *everything;
	gsize len;

	printf("Trying stylesheet '%s'\n", g_file_get_uri(file));

	ss = calloc(1, sizeof(Stylesheet));
	if ( ss == NULL ) return NULL;

	parser = json_parser_new();

	if ( !g_file_load_contents(file, NULL, &everything, &len, NULL, NULL) ) {
		fprintf(stderr, _("Failed to load stylesheet '%s'\n"),
		        g_file_get_uri(file));
		return NULL;
	}

	r = json_parser_load_from_data(parser, everything, len, &err);
	if ( r == FALSE ) {
		fprintf(stderr, "Failed to load style sheet: '%s'\n", err->message);
		return NULL;
	}

	ss->root = json_parser_steal_root(parser);
	g_object_unref(parser);

	return ss;
}


static JsonObject *find_stylesheet_object(Stylesheet *ss, const char *path,
                                          JsonNode **freeme)
{
	JsonNode *node;
	JsonObject *obj;
	JsonArray *array;
	GError *err = NULL;

	node = json_path_query(path, ss->root, &err);
	array = json_node_get_array(node);

	if ( json_array_get_length(array) != 1 ) {
		json_node_unref(node);
		fprintf(stderr, "More than one result in SS lookup (%s)!\n", path);
		return NULL;
	}

	obj = json_array_get_object_element(array, 0);
	if ( obj == NULL ) {
		printf("%s not a JSON object\n", path);
		json_node_unref(node);
		return NULL;
	}

	*freeme = node;
	return obj;
}


char *stylesheet_lookup(Stylesheet *ss, const char *path, const char *key)
{
	JsonObject *obj;
	char *ret = NULL;
	JsonNode *node = NULL;

	if ( ss == NULL ) {
		fprintf(stderr, _("No stylesheet!\n"));
		return NULL;
	}

	obj = find_stylesheet_object(ss, path, &node);

	if ( json_object_has_member(obj, key) ) {

		const gchar *v;
		v = json_object_get_string_member(obj, key);
		if ( v != NULL ) {
			ret = strdup(v);
		} else {
			fprintf(stderr, "Error retrieving %s.%s\n", path, key);
		}

	} /* else not found, too bad */

	if ( node != NULL ) json_node_unref(node);
	return ret;
}


int stylesheet_set(Stylesheet *ss, const char *path, const char *key,
                    const char *new_val)
{
	JsonObject *obj;
	JsonNode *node = NULL;
	int r = 1;

	if ( ss == NULL ) {
		fprintf(stderr, _("No stylesheet!\n"));
		return 1;
	}

	obj = find_stylesheet_object(ss, path, &node);
	if ( obj != NULL ) {
		json_object_set_string_member(obj, key, new_val);
		r = 0;
	} /* else most likely the object (e.g. "$.slide", "$.slide.frame",
	   * "$.narrative" etc doesn't exist */

	if ( node != NULL ) json_node_unref(node);
	return r;
}


int stylesheet_delete(Stylesheet *ss, const char *path, const char *key)
{
	JsonObject *obj;
	JsonNode *node = NULL;
	int r = 1;

	if ( ss == NULL ) {
		fprintf(stderr, _("No stylesheet!\n"));
		return 1;
	}

	obj = find_stylesheet_object(ss, path, &node);
	if ( obj != NULL ) {
		json_object_remove_member(obj, key);
		r = 0;
	} /* else most likely the object (e.g. "$.slide", "$.slide.frame",
	   * "$.narrative" etc doesn't exist */

	if ( node != NULL ) json_node_unref(node);
	return r;
}


void stylesheet_free(Stylesheet *ss)
{
	g_object_unref(ss->root);
	free(ss);
}


int stylesheet_save(Stylesheet *ss, GFile *file)
{
	JsonGenerator *gen;
	GError *error = NULL;
	GFileOutputStream *fh;

	fh = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
	if ( fh == NULL ) {
		fprintf(stderr, _("Open failed: %s\n"), error->message);
		return 1;
	}

	gen = json_generator_new();
	json_generator_set_root(gen, ss->root);
	json_generator_set_pretty(gen, TRUE);
	json_generator_set_indent(gen, 1);
	json_generator_set_indent_char(gen, '\t');
	if ( !json_generator_to_stream(gen, G_OUTPUT_STREAM(fh), NULL, &error) ) {
		fprintf(stderr, _("Open failed: %s\n"), error->message);
		return 1;
	}
	g_object_unref(fh);
	return 0;
}
