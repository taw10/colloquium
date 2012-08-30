/*
 * storycode.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "storycode.h"
#include "presentation.h"


struct _scblocklist
{
	int n_blocks;
	int max_blocks;

	struct scblock *blocks;
};


struct _scblocklistiterator
{
	int pos;
};


static int allocate_blocks(SCBlockList *bl)
{
	struct scblock *blocks_new;

	blocks_new = realloc(bl->blocks, bl->max_blocks*sizeof(struct scblock));
	if ( blocks_new == NULL ) {
		return 1;
	}
	bl->blocks = blocks_new;

	return 0;
}


SCBlockList *sc_block_list_new()
{
	SCBlockList *bl;

	bl = calloc(1, sizeof(SCBlockList));
	if ( bl == NULL ) return NULL;

	bl->n_blocks = 0;
	bl->max_blocks = 64;
	bl->blocks = NULL;
	if ( allocate_blocks(bl) ) {
		free(bl);
		return NULL;
	}

	return bl;
}


void sc_block_list_free(SCBlockList *bl)
{
	int i;

	for ( i=0; i<bl->n_blocks; i++ ) {
		free(bl->blocks[i].name);
		free(bl->blocks[i].options);
		free(bl->blocks[i].contents);
	}
	free(bl->blocks);
	free(bl);
}


struct scblock *sc_block_list_first(SCBlockList *bl,
                                    SCBlockListIterator **piter)
{
	SCBlockListIterator *iter;

	if ( bl->n_blocks == 0 ) return NULL;

	iter = calloc(1, sizeof(SCBlockListIterator));
	if ( iter == NULL ) return NULL;

	iter->pos = 0;
	*piter = iter;

	return &bl->blocks[0];
}


struct scblock *sc_block_list_next(SCBlockList *bl, SCBlockListIterator *iter)
{
	iter->pos++;
	if ( iter->pos == bl->n_blocks ) {
		free(iter);
		return NULL;
	}

	return &bl->blocks[iter->pos];
}


static int sc_block_list_add(SCBlockList *bl,
                             char *name, char *options, char *contents)
{
	if ( bl->n_blocks == bl->max_blocks ) {
		bl->max_blocks += 64;
		if ( allocate_blocks(bl) ) return 1;
	}

	bl->blocks[bl->n_blocks].name = name;
	bl->blocks[bl->n_blocks].options = options;
	bl->blocks[bl->n_blocks].contents = contents;
	bl->n_blocks++;
	return 0;
}


SCBlockList *sc_find_blocks(const char *sc, const char *blockname)
{
	SCBlockList *bl;
	const char *pos;
	char label[1024];

	bl = sc_block_list_new();
	if ( bl == NULL ) return NULL;

	if ( strlen(blockname) > 1021 ) {
		fprintf(stderr, "Block name '%s' too long.\n", blockname);
		return NULL;
	}

	strcpy(label, "\\");
	strcat(label, blockname);
	strcat(label, "{");

	pos = sc;
	do {

		pos = strstr(pos, label);

		if ( pos != NULL ) {

			int i;
			int bct = 1;
			int found = 0;
			int ml = strlen(pos);

			pos += strlen(label);

			for ( i=0; i<ml; i++ ) {
				if ( pos[i] == '{' ) {
					bct++;
				} else if ( pos[i] == '}' ) {
					bct--;
				}
				if ( bct == 0 ) {
					found = 1;
					break;
				}
			}

			if ( (!found) || (bct != 0) ) {
				fprintf(stderr, "Parse error while looking for"
				                " block '%s'\n", blockname);
				sc_block_list_free(bl);
				return NULL;
			}

			/* FIXME: Find options */

			if ( sc_block_list_add(bl, strdup(blockname), NULL,
			                       strndup(pos, i)) )
			{
				fprintf(stderr, "Failed to add block.\n");
				sc_block_list_free(bl);
				return NULL;
			}

			pos += i+1;
			printf("Remaining text '%s'\n", pos);

		}

	} while ( pos != NULL );

	return bl;
}


/* Unpack level 2 StoryCode (content + subframes) into frames */
struct frame *sc_unpack(const char *sc)
{
	struct frame *fr;

	fr = calloc(1, sizeof(struct frame));
	if ( fr == NULL ) return NULL;



	return fr;
}
