/*
 * scparse_priv.h
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

#ifndef SCPARSE_PRIV_H
#define SCPARSE_PRIV_H

#include "narrative.h"
#include "slide.h"

enum style_mask
{
	STYMASK_GEOM = 1<<0,
	STYMASK_FONT = 1<<1,
	STYMASK_ALIGNMENT = 1<<2,
	STYMASK_PADDING = 1<<3,
	STYMASK_PARASPACE = 1<<4,
	STYMASK_FGCOL = 1<<5,
	STYMASK_BGCOL = 1<<6,
};

struct scpctx
{
	Narrative *n;
	Slide *s;

	int n_str;
	int max_str;
	char **str;

	/* Current style or frame options.
	 * These will be copied to a stylesheet entry or frame when the
	 * top-level rule is matched. */
	enum style_mask mask;
	struct frame_geom geom;
	char *font;
	enum alignment alignment;
	struct length padding[4];
	struct length paraspace[4];
	struct colour fgcol;
	enum gradient bggrad;
	struct colour bgcol;
	struct colour bgcol2;
};

#endif /* SCPARSE_PRIV_H */
