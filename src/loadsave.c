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


#include "presentation.h"
#include "objects.h"
#include "stylesheet.h"


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


static void write_stylesheet(StyleSheet *ss, FILE *fh)
{
}


int save_presentation(struct presentation *p, const char *filename)
{
	FILE *fh;
	int i;

	fh = fopen(filename, "w");
	if ( fh == NULL ) return 1;

	fprintf(fh, "# Colloquium presentation file\n");
	fprintf(fh, "version=0\n");

	write_stylesheet(p->ss, fh);

	for ( i=0; i<p->num_slides; i++ ) {

		int j;
		struct slide *s;

		s = p->slides[i];

		fprintf(fh, "++slide\n");
		fprintf(fh, "width=%.2f\n", s->slide_width);
		fprintf(fh, "height=%.2f\n", s->slide_height);

		for ( j=0; j<s->num_objects; j++ ) {

			struct object *o;

			o = s->objects[j];

			if ( o->empty ) continue;

			fprintf(fh, "++object\n");
			fprintf(fh, "type=%s\n", type_text(o->type));
			fprintf(fh, "x=%.2f\n", o->x);
			fprintf(fh, "y=%.2f\n", o->y);
			fprintf(fh, "w=%.2f\n", o->bb_width);
			fprintf(fh, "h=%.2f\n", o->bb_height);
			fprintf(fh, "--object\n");
		}

		fprintf(fh, "--slide\n");

	}

	fclose(fh);
	return 0;
}
