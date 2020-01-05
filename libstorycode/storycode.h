/*
 * storycode.h
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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

#ifndef STORYCODE_H
#define STORYCODE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

enum text_run_type
{
	TEXT_RUN_NORMAL,
	TEXT_RUN_BOLD,
	TEXT_RUN_ITALIC,
	TEXT_RUN_UNDERLINE,
};

struct text_run
{
	enum text_run_type type;
	char *text;
};


#include "narrative.h"

extern const char *alignc(enum alignment ali);
extern const char *bgcolc(enum gradient bggrad);
extern char unitc(enum length_unit unit);

extern Narrative *storycode_parse_presentation(const char *sc);
extern int storycode_write_presentation(Narrative *n, GOutputStream *fh);
extern int narrative_write_item(Narrative *n, int item, GOutputStream *fh);
extern int narrative_write_partial_item(Narrative *n, int inum, size_t start, ssize_t end,
                                        GOutputStream *fh);

#endif /* STORYCODE_H */
