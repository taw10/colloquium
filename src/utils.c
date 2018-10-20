/*
 * utils.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void chomp(char *s)
{
	size_t i;

	if ( !s ) return;

	for ( i=0; i<strlen(s); i++ ) {
		if ( (s[i] == '\n') || (s[i] == '\r') ) {
			s[i] = '\0';
			return;
		}
	}
}


int safe_strcmp(const char *a, const char *b)
{
	if ( a == NULL ) return 1;
	if ( b == NULL ) return 1;
	return strcmp(a, b);
}


int parse_double(const char *a, float v[2])
{
	int nn;

	nn = sscanf(a, "%fx%f", &v[0], &v[1]);
	if ( nn != 2 ) {
		fprintf(stderr, _("Invalid size '%s'\n"), a);
		return 1;
	}

	return 0;
}


int parse_tuple(const char *a, float v[4])
{
	int nn;

	nn = sscanf(a, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
	if ( nn != 4 ) {
		fprintf(stderr, _("Invalid tuple '%s'\n"), a);
		return 1;
	}

	return 0;
}

