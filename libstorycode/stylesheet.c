/*
 * stylesheet.c
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

#include "stylesheet.h"

struct style
{
	struct frame_geom geom;
	char *font;
	double fgcol[4];      /* r g b a */
	double bgcol[4];      /* r g b a */
	double bgcol2[4];     /* r g b a, if gradient */
	struct length paraspace[4];  /* l r t b */
	struct length padding[4];    /* l r t b */
	enum alignment alignment;
};


struct _stylesheet
{
	struct style narrative;

	double default_slide_w;
	double default_slide_h;
	struct style slide_text;
	struct style slide_prestitle;
	struct style slide_slidetitle;
};


static void default_style(struct style *s)
{
	s->geom.x.len = 0.0;
	s->geom.x.unit = LENGTH_FRAC;
	s->geom.y.len = 0.0;
	s->geom.y.unit = LENGTH_FRAC;
	s->geom.w.len = 1.0;
	s->geom.w.unit = LENGTH_FRAC;
	s->geom.h.len = 1.0;
	s->geom.h.unit = LENGTH_FRAC;

	s->font = strdup("Sans 12");

	s->fgcol[0] = 0.0;
	s->fgcol[1] = 0.0;
	s->fgcol[2] = 0.0;
	s->fgcol[3] = 1.0;

	s->bgcol[0] = 1.0;
	s->bgcol[1] = 1.0;
	s->bgcol[2] = 1.0;
	s->bgcol[3] = 1.0;

	s->bgcol2[0] = 1.0;
	s->bgcol2[1] = 1.0;
	s->bgcol2[2] = 1.0;
	s->bgcol2[3] = 1.0;

	s->paraspace[0].len = 0.0;
	s->paraspace[1].len = 0.0;
	s->paraspace[2].len = 0.0;
	s->paraspace[3].len = 0.0;
	s->paraspace[0].unit = LENGTH_UNIT;
	s->paraspace[1].unit = LENGTH_UNIT;
	s->paraspace[2].unit = LENGTH_UNIT;
	s->paraspace[3].unit = LENGTH_UNIT;

	s->padding[0].len = 0.0;
	s->padding[1].len = 0.0;
	s->padding[2].len = 0.0;
	s->padding[3].len = 0.0;
	s->padding[0].unit = LENGTH_UNIT;
	s->padding[1].unit = LENGTH_UNIT;
	s->padding[2].unit = LENGTH_UNIT;
	s->padding[3].unit = LENGTH_UNIT;
}


Stylesheet *stylesheet_new()
{
	Stylesheet *s;
	s = malloc(sizeof(*s));
	if ( s == NULL ) return NULL;

	/* Ultimate defaults */
	default_style(&s->narrative);
	default_style(&s->slide_text);
	default_style(&s->slide_prestitle);
	default_style(&s->slide_slidetitle);

	return s;
}

void stylesheet_free(Stylesheet *s)
{
	free(s);
}


static struct style *get_style(Stylesheet *s, enum style_element el)
{
	if ( s == NULL ) return NULL;
	switch ( el ) {
		case STYEL_NARRATIVE : return &s->narrative;
		case STYEL_SLIDE_TEXT : return &s->slide_text;
		case STYEL_SLIDE_PRESTITLE : return &s->slide_prestitle;
		case STYEL_SLIDE_SLIDETITLE : return &s->slide_slidetitle;
		default : return NULL;
	}
}


int stylesheet_set_slide_default_size(Stylesheet *s, double w, double h)
{
	if ( s == NULL ) return 1;
	s->default_slide_w = w;
	s->default_slide_h = h;
	return 0;
}


int stylesheet_set_geometry(Stylesheet *s, enum style_element el, struct frame_geom geom)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	sty->geom = geom;
	return 0;
}


int stylesheet_set_font(Stylesheet *s, enum style_element el, char *font)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	if ( sty->font != NULL ) {
		free(sty->font);
	}
	sty->font = font;
	return 0;
}


int stylesheet_set_padding(Stylesheet *s, enum style_element el, struct length padding[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->padding[i] = padding[i];
	}
	return 0;
}


int stylesheet_set_paraspace(Stylesheet *s, enum style_element el, struct length paraspace[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->paraspace[i] = paraspace[i];
	}
	return 0;
}


int stylesheet_set_fgcol(Stylesheet *s, enum style_element el, double rgba[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->fgcol[i] = rgba[i];
	}
	return 0;
}


int stylesheet_set_bgcol(Stylesheet *s, enum style_element el, double rgba[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->bgcol[i] = rgba[i];
	}
	return 0;
}


int stylesheet_set_alignment(Stylesheet *s, enum style_element el, enum alignment align)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	sty->alignment = align;
	return 0;
}


const char *stylesheet_get_slide_text_font(Stylesheet *s)
{
	if ( s == NULL ) return NULL;
	return s->slide_text.font;
}
