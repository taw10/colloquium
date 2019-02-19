/*
 * slide.c
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

#include "slide.h"

enum slide_item_type
{
	SLIDE_ITEM_TEXT,
};


struct slide_item
{
	enum slide_item_type type;

	/* For SLIDE_ITEM_TEXT */
	char **paragraphs;
	int n_paras;
};


struct _slide
{
	int n_items;
	struct slide_item *items;
};


Slide *slide_new()
{
	Slide *s;
	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;
	s->n_items = 0;
	s->items = NULL;
	return s;
}

void slide_free(Slide *s)
{
	free(s->items);
	free(s);
}


int slide_add_prestitle(Slide *s, char *prestitle)
{
	return 0;
}


int slide_add_image(Slide *s, char *filename, struct frame_geom geom)
{
	return 0;
}


int slide_add_text(Slide *s, char *text, struct frame_geom geom)
{
	return 0;
}


int slide_add_footer(Slide *s)
{
	return 0;
}


int slide_add_slidetitle(Slide *s, char *slidetitle)
{
	return 0;
}
