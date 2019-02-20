/*
 * presentation.c
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
#include <assert.h>
#include <stdio.h>

#include "presentation.h"
#include "stylesheet.h"
#include "slide.h"
#include "narrative.h"

struct _presentation
{
	Stylesheet *stylesheet;
	Narrative *narrative;
	int n_slides;
	Slide **slides;
	int max_slides;
};


Presentation *presentation_new()
{
	Presentation *p;
	p = malloc(sizeof(*p));
	if ( p == NULL ) return NULL;
	p->stylesheet = NULL;
	p->narrative = NULL;
	p->slides = NULL;
	p->n_slides = 0;
	p->max_slides = 0;
	return p;
}


void presentation_free(Presentation *p)
{
	free(p);
}


void presentation_add_stylesheet(Presentation *p, Stylesheet *ss)
{
	assert(p->stylesheet == NULL);
	p->stylesheet = ss;
}


void presentation_add_narrative(Presentation *p, Narrative *n)
{
	assert(p->narrative == NULL);
	p->narrative = n;
}


void presentation_add_slide(Presentation *p, Slide *s)
{
	assert(p->n_slides <= p->max_slides);
	if ( p->n_slides == p->max_slides ) {
		Slide **nslides = realloc(p->slides,
		                          (p->max_slides+8)*sizeof(Slide *));
		if ( nslides == NULL ) {
			fprintf(stderr, "Failed to allocate memory for slide\n");
			return;
		}
		p->slides = nslides;
		p->max_slides += 8;
	}

	p->slides[p->n_slides++] = s;
}


int presentation_num_slides(Presentation *p)
{
	return p->n_slides;
}


Slide *presentation_slide(Presentation *p, int i)
{
	if ( i >= p->n_slides ) return NULL;
	if ( i < 0 ) return NULL;
	return p->slides[i];
}
