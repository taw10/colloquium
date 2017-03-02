/*
 * sc_parse.h
 *
 * Copyright © 2013-2017 Thomas White <taw@bitwiz.org.uk>
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

extern SCBlock *sc_block_copy(const SCBlock *bl);

extern SCBlock *sc_block_next(const SCBlock *bl);
extern SCBlock *sc_block_child(const SCBlock *bl);
extern const char *sc_block_name(const SCBlock *bl);
extern const char *sc_block_options(const SCBlock *bl);
extern const char *sc_block_contents(const SCBlock *bl);
extern void sc_block_substitute(SCBlock **top, SCBlock *old, SCBlock *new);

extern SCBlock *sc_block_append(SCBlock *bl,
                                char *name, char *opt, char *contents,
                                SCBlock **blfp);

extern void sc_block_append_p(SCBlock *bl, SCBlock *bln);

extern SCBlock *sc_block_append_end(SCBlock *bl,
                                    char *name, char *opt, char *contents);

extern SCBlock *sc_block_append_inside(SCBlock *parent,
                                       char *name, char *opt, char *contents);

extern SCBlock *sc_block_insert_after(SCBlock *afterme,
                                      char *name, char *opt, char *contents);

extern void sc_block_delete(SCBlock **top, SCBlock *deleteme);
extern void sc_block_unlink(SCBlock **top, SCBlock *deleteme);

extern SCBlock *find_last_child(SCBlock *bl);


extern void sc_block_set_options(SCBlock *bl, char *opt);
extern void sc_block_set_contents(SCBlock *bl, char *con);
extern void sc_insert_text(SCBlock *b1, size_t o1, const char *t);
extern void sc_insert_block(SCBlock *b1, int o1, SCBlock *ins);
extern SCBlock *sc_block_split(SCBlock *bl, size_t pos);

extern void show_sc_blocks(const SCBlock *bl);
extern void show_sc_block(const SCBlock *bl, const char *prefix);

extern char *serialise_sc_block(const SCBlock *bl);
extern void save_sc_block(FILE *fh, const SCBlock *bl);

extern void scblock_delete_text(SCBlock *b, size_t o1, size_t o2);

#endif	/* SC_PARSE_H */
