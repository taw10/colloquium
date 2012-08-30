/*
 * storycode.h
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

#ifndef STORYCODE_H
#define STORYCODE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _scblocklist SCBlockList;
typedef struct _scblocklistiterator SCBlockListIterator;

struct scblock
{
	char *name;
	char *options;
	char *contents;
};

struct scblock *sc_block_list_first(SCBlockList *bl,
                                    SCBlockListIterator **piter);
struct scblock *sc_block_list_next(SCBlockList *bl, SCBlockListIterator *iter);

extern SCBlockList *sc_find_blocks(const char *sc, const char *blockname);
extern void sc_block_list_free(SCBlockList *bl);

#endif	/* STORYCODE_H */
