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


static char *fgets_long(FILE *fh, size_t *lp)
{
	char *line;
	size_t la;
	size_t l = 0;

	la = 1024;
	line = malloc(la);
	if ( line == NULL ) return NULL;

	do {

		int r;

		r = fgetc(fh);
		if ( r == EOF ) {
			if ( l == 0 ) {
				free(line);
				*lp = 0;
				return NULL;
			} else {
				line[l++] = '\0';
				*lp = l;
				return line;
			}
		}

		line[l++] = r;

		if ( r == '\n' ) {
			line[l++] = '\0';
			*lp = l;
			return line;
		}

		if ( l == la ) {

			char *ln;

			la += 1024;
			ln = realloc(line, la);
			if ( ln == NULL ) {
				free(line);
				*lp = 0;
				return NULL;
			}

			line = ln;

		}

	} while ( 1 );
}


char *load_everything(const char *filename)
{
	FILE *fh;
	size_t el = 1;
	char *everything = strdup("");

	fh = fopen(filename, "r");
	if ( fh == NULL ) return NULL;

	while ( !feof(fh) ) {

		size_t len = 0;
		char *line = fgets_long(fh, &len);

		if ( line != NULL ) {

			everything = realloc(everything, el+len);
			if ( everything == NULL ) {
				fprintf(stderr, "Failed to allocate memory\n");
				return NULL;
			}
			el += len;

			strcat(everything, line);
		}

	}

	fclose(fh);

	return everything;
}
