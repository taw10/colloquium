/*
 * stylesheet.h
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

#ifndef STYLESHEET_H
#define STYLESHEET_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _stylesheet Stylesheet;

enum alignment
{
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTER
};


enum length_unit
{
	LENGTH_FRAC,
	LENGTH_UNIT
};


struct length
{
	double len;
	enum length_unit unit;
};


struct frame_geom
{
	struct length x;
	struct length y;
	struct length w;
	struct length h;
};


enum gradient
{
	GRAD_NONE,
	GRAD_HORIZ,
	GRAD_VERT
};


enum style_element
{
	STYEL_NARRATIVE,
	STYEL_SLIDE,
	STYEL_SLIDE_TEXT,
	STYEL_SLIDE_PRESTITLE,
	STYEL_SLIDE_SLIDETITLE,
};


extern Stylesheet *stylesheet_new(void);
extern void stylesheet_free(Stylesheet *s);

extern int stylesheet_set_slide_default_size(Stylesheet *s, double w, double h);

extern int stylesheet_set_geometry(Stylesheet *s, enum style_element el, struct frame_geom geom);
extern int stylesheet_set_font(Stylesheet *s, enum style_element el, char *font);
extern int stylesheet_set_alignment(Stylesheet *s, enum style_element el, enum alignment align);
extern int stylesheet_set_padding(Stylesheet *s, enum style_element el, struct length padding[4]);
extern int stylesheet_set_paraspace(Stylesheet *s, enum style_element el, struct length paraspace[4]);
extern int stylesheet_set_fgcol(Stylesheet *s, enum style_element el, double rgba[4]);
extern int stylesheet_set_bgcol(Stylesheet *s, enum style_element el, double rgba[4]);

extern const char *stylesheet_get_slide_text_font(Stylesheet *s);


#endif /* STYLESHEET_H */
