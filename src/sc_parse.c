/*
 * sc_parse.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "sc_parse.h"

struct scblock
{
	char *name;
	char *options;
	char *contents;

	SCBlock *next;
	SCBlock *prev;
	SCBlock *child;

	size_t offset;
};


SCBlock *sc_block_new()
{
	SCBlock *bl;

	bl = calloc(1, sizeof(SCBlock));
	if ( bl == NULL ) return NULL;

	return bl;
}


/* Insert a new block after "bl" */
SCBlock *sc_block_insert(SCBlock *bl)
{
	SCBlock *bl = sc_block_new();

	/* FIXME: Implementation */

	return bl;
}


/* Frees "bl" and all its children, and links it out of its chain */
void sc_block_free(SCBlock *bl)
{
	if ( bl->child != NULL ) {
		SCBlock *ch = bl->child;
		while ( ch != NULL ) {
			sc_block_free(ch);
			ch = ch->next;
		}
	}

	free(bl);
}


static int get_subexpr(const char *sc, char *bk, char **pcontents, int *err)
{
	size_t ml;
	int i;
	int bct = 1;
	int found = 0;
	char *contents;

	*err = 0;

	ml = strlen(sc);
	contents = malloc(ml+1);
	if ( contents == NULL ) {
		*err = -1;
		return 0;
	}
	*pcontents = contents;

	for ( i=0; i<ml; i++ ) {
		if ( sc[i] == bk[0] ) {
			bct++;
		} else if ( sc[i] == bk[1] ) {
			bct--;
		}
		if ( bct == 0 ) {
			found = 1;
			break;
		}
		contents[i] = sc[i];
	}

	if ( (!found) || (bct != 0) ) {
		*err = 1;
		return 0;
	}

	contents[i] = '\0';
	return i+1;
}


static size_t read_block(const char *sc, char **pname, char **options,
                         char **contents, int *err)
{
	size_t l, i, j;
	char *name;
	int done;

	*err = 0;

	l = strlen(sc);
	i = 0;  j = 0;
	name = malloc(l+1);
	if ( name == NULL ) {
		*err = 1;
		return 0;
	}

	done = 0;
	do {

		char c = sc[i];

		if ( isalnum(c) ) {
			name[j++] = c;
			i++;
		} else {
			/* Found the end of the name */
			done = 1;
		}

	} while ( !done && (i<l) );

	name[j] = '\0';
	*pname = name;

	if ( sc[i] == '[' ) {

		i += get_subexpr(sc+i+1, "[]", options, err) + 1;
		if ( *err ) {
			printf("Couldn't find end of options '%s'\n", sc+i);
			return 0;
		}

	} else {
		*options = NULL;
	}

	if ( sc[i] == '{' ) {

		i += get_subexpr(sc+i+1, "{}", contents, err) + 1;
		if ( *err ) {
			printf("Couldn't find end of content '%s'\n", sc+i);
			return 0;
		}

	} else {
		*contents = NULL;
	}

	return i+1;
}


SCBlock *sc_parse(const char *sc)
{
	SCBlock *bl;
	char *tbuf;
	size_t len, i, j, start;

	if ( sc == NULL ) return NULL;

	bl = sc_block_new();
	if ( bl == NULL ) return NULL;

	len = strlen(sc);
	tbuf = malloc(len+1);
	if ( tbuf == NULL ) {
		sc_block_free(bl);
		return NULL;
	}

	i = 0;  j = 0;  start = 0;
	do {

		if ( sc[i] == '\\' ) {

			int err;
			char *name = NULL;
			char *options = NULL;
			char *contents = NULL;

			if ( (blockname == NULL) && (j != 0) ) {
				tbuf[j] = '\0';
				if ( sc_block_list_add(bl, i, NULL, NULL,
				                       strdup(tbuf)) )
				{
					fprintf(stderr,
					        "Failed to add block.\n");
					sc_block_list_free(bl);
					free(tbuf);
					return NULL;
				}
				j = 0;
			}

			i += read_block(sc+i+1, &name, &options, &contents,
			                &err);
			if ( err ) {
				printf("Parse error\n");
				sc_block_list_free(bl);
				free(tbuf);
				return NULL;
			}

			if ( (blockname == NULL)
			  || ((blockname != NULL) && !strcmp(blockname, name)) )
			{
				if ( sc_block_list_add(bl, i, name, options,
				                       contents) )
				{
					fprintf(stderr,
					        "Failed to add block.\n");
					sc_block_list_free(bl);
					free(tbuf);
					return NULL;
				}
			}

			/* Start of the next block */
			start = i;


		} else {

			tbuf[j++] = sc[i++];
		}

	} while ( i<len );

	/* Add final block, if it exists */
	if ( (blockname == NULL) && (j > 0) ) {

		/* Leftover buffer is empty? */
		if ( (j==1) && (tbuf[0]=='\0') ) return bl;

		tbuf[j] = '\0';
		if ( sc_block_list_add(bl, start, NULL, NULL, tbuf) )
		{
			fprintf(stderr,
			        "Failed to add block.\n");
			sc_block_list_free(bl);
			free(tbuf);
			return NULL;
		}
		j = 0;
	}

	return bl;
}
