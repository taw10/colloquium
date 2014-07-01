/*
 * sc_parse.h
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

#ifndef SC_PARSE_H
#define SC_PARSE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

typedef struct _scblock SCBlock;

extern SCBlock *sc_parse(const char *sc);

extern void sc_block_free(SCBlock *bl);

extern SCBlock *sc_block_next(const SCBlock *bl);
extern SCBlock *sc_block_child(const SCBlock *bl);
extern const char *sc_block_name(const SCBlock *bl);
extern const char *sc_block_options(const SCBlock *bl);
extern const char *sc_block_contents(const SCBlock *bl);

extern struct frame *sc_block_frame(const SCBlock *bl);
extern void sc_block_set_frame(SCBlock *bl, struct frame *fr);

extern void sc_insert_text(SCBlock *b1, int o1, const char *t);
extern void sc_delete_text(SCBlock *b1, int o1, SCBlock *b2, int o2);

extern void show_sc_blocks(const SCBlock *bl);
extern void show_sc_block(const SCBlock *bl, const char *prefix);

extern void save_sc_block(FILE *fh, const SCBlock *bl);

#endif	/* SC_PARSE_H */
