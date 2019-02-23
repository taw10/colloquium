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

#include "presentation.h"
#include "narrative.h"
#include "slide.h"

struct scpctx
{
	Presentation *p;
	Narrative *n;
	Stylesheet *ss;
	Slide *s;

	int n_str;
	int max_str;
	char **str;

	/* Frame options */
	struct frame_geom geom;
	int geom_set;
	char *font;
};

#endif /* SCPARSE_PRIV_H */