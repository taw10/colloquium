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
#include <ctype.h>

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

	if ( !done ) {
		*err = 1;
		printf("Couldn't find end of block beginning '%s'\n", sc);
		return 0;
	}
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


SCBlockList *sc_find_blocks(const char *sc, const char *blockname)
{
	SCBlockList *bl;
	char *tbuf;
	size_t len, i, j;

	bl = sc_block_list_new();
	if ( bl == NULL ) return NULL;

	len = strlen(sc);
	tbuf = malloc(len+1);
	if ( tbuf == NULL ) {
		sc_block_list_free(bl);
		return NULL;
	}

	i = 0;  j = 0;
	do {

		if ( sc[i] == '\\' ) {

			int err;
			char *name = NULL;
			char *options = NULL;
			char *contents = NULL;

			if ( (blockname == NULL) && (j != 0) ) {
				tbuf[j] = '\0';
				if ( sc_block_list_add(bl, NULL, NULL,
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
				if ( sc_block_list_add(bl, name, options,
				                       contents) )
				{
					fprintf(stderr,
					        "Failed to add block.\n");
					sc_block_list_free(bl);
					free(tbuf);
					return NULL;
				}
			}

		} else {

			tbuf[j++] = sc[i++];
		}

	} while ( i<len );

	if ( (blockname == NULL) && (j != 0) ) {
		tbuf[j] = '\0';
		if ( sc_block_list_add(bl, NULL, NULL, tbuf) )
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


static char *remove_blocks(const char *in, const char *blockname)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	char *out;
	struct scblock *b;

	bl = sc_find_blocks(in, NULL);
	if ( bl == NULL ) {
		printf("Failed to find blocks.\n");
		return NULL;
	}

	out = malloc(strlen(in)+1);
	if ( out == NULL ) return NULL;
	out[0] = '\0';

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		if ( b->name == NULL ) {
			strcat(out, b->contents);
		} else {

			if ( strcmp(blockname, b->name) != 0 ) {
				strcat(out, "\\");
				strcat(out, b->name);
				if ( b->options != NULL ) {
					strcat(out, "[");
					strcat(out, b->options);
					strcat(out, "]");
				}
				if ( b->contents != NULL ) {
					strcat(out, "{");
					strcat(out, b->contents);
					strcat(out, "}");
				}

				if ( (b->options == NULL)
				  && (b->contents == NULL) ) {
					strcat(out, " ");
				}

			}

		}
	}
	sc_block_list_free(bl);

	return out;
}


static int alloc_ro(struct frame *fr)
{
	struct frame **new_ro;

	new_ro = realloc(fr->rendering_order,
	                 fr->max_ro*sizeof(struct frame *));
	if ( new_ro == NULL ) return 1;

	fr->rendering_order = new_ro;

	return 0;
}


static struct frame *frame_new()
{
	struct frame *n;

	n = calloc(1, sizeof(struct frame));
	if ( n == NULL ) return NULL;

	n->rendering_order = NULL;
	n->max_ro = 32;
	alloc_ro(n);

	n->num_ro = 1;
	n->rendering_order[0] = n;

	return n;
}


static struct frame *add_subframe(struct frame *fr)
{
	struct frame *n;

	n = frame_new();
	if ( n == NULL ) return NULL;

	if ( fr->num_ro == fr->max_ro ) {
		fr->max_ro += 32;
		if ( alloc_ro(fr) ) return NULL;
	}

	fr->rendering_order[fr->num_ro++] = n;

	return n;
}


static void recursive_unpack(struct frame *fr, const char *sc)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;

	bl = sc_find_blocks(sc, "f");

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		struct frame *sfr;
		sfr = add_subframe(fr);
		sfr->sc = remove_blocks(b->contents, "f");
		recursive_unpack(sfr, b->contents);
	}
	sc_block_list_free(bl);
}


static void show_heirarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];

	strcpy(tn, t);
	strcat(tn, "  |-> ");

	printf("%s%p %s\n", t, fr, fr->sc);

	for ( i=0; i<fr->num_ro; i++ ) {
		if ( fr->rendering_order[i] != fr ) {
			show_heirarchy(fr->rendering_order[i], tn);
		} else {
			printf("%s<this frame>\n", tn);
		}
	}

}


/* Unpack level 2 StoryCode (content + subframes) into frames */
struct frame *sc_unpack(const char *sc)
{
	struct frame *fr;

	fr = frame_new();
	if ( fr == NULL ) return NULL;

	fr->sc = remove_blocks(sc, "f");
	recursive_unpack(fr, sc);

	show_heirarchy(fr, "");

	return fr;
}
