/*
 * presentation.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "presentation.h"


struct presentation *new_presentation()
{
	struct presentation *new;

	new = malloc(sizeof(struct presentation));

	new->titlebar = strdup("(untitled)");
	new->filename = NULL;

	new->window = NULL;
	new->ui = NULL;
	new->action_group = NULL;

	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	return new;
}
