/*
 * loadsave.v
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

#include "presentation.h"
#include "objects.h"
#include "stylesheet.h"


struct serializer
{
	FILE *fh;

	char *stack[32];
	int stack_depth;
	char *prefix;
	int empty_set;
	int blank_written;
};


int load_presentation(struct presentation *p, const char *filename)
{
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
		fprintf(ser->fh, "\n");
		fprintf(ser->fh, "[%s]\n", ser->prefix);
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
	fprintf(ser->fh, "%s = %.2ff\n", key, val);
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
			//o->serialize(o, &ser);
			serialize_end(&ser);

		}
		serialize_end(&ser);

	}
	serialize_end(&ser);

	fclose(fh);
	return 0;
}
