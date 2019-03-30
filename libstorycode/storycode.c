/*
 * storycode.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "narrative.h"
#include "slide.h"
#include "stylesheet.h"

#include "storycode_parse.h"
#include "storycode_lex.h"
#include "scparse_priv.h"


Narrative *storycode_parse_presentation(const char *sc)
{
	YY_BUFFER_STATE b;
	struct scpctx parse_ctx;

	b = sc_scan_string(sc);
	scparse(&parse_ctx);
	sc_delete_buffer(b);

	return parse_ctx.n;
}
