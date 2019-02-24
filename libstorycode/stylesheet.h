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


extern Stylesheet *stylesheet_new(void);
extern void stylesheet_free(Stylesheet *s);

extern int stylesheet_set_slide_default_size(Stylesheet *s, double w, double h);

extern int stylesheet_set_slide_text_font(Stylesheet *s, char *font);
extern int stylesheet_set_slide_text_pad(Stylesheet *s, double padding[4]);
extern int stylesheet_set_slide_text_paraspace(Stylesheet *s, double paraspace[4]);
extern int stylesheet_set_slide_text_fgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_slide_text_bgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_slide_text_align(Stylesheet *s, enum alignment align);

extern int stylesheet_set_slide_prestitle_geom(Stylesheet *s, struct frame_geom geom);
extern int stylesheet_set_slide_prestitle_font(Stylesheet *s, char *font);
extern int stylesheet_set_slide_prestitle_pad(Stylesheet *s, double padding[4]);
extern int stylesheet_set_slide_prestitle_paraspace(Stylesheet *s, double paraspace[4]);
extern int stylesheet_set_slide_prestitle_fgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_slide_prestitle_bgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_slide_prestitle_align(Stylesheet *s, enum alignment align);

extern int stylesheet_set_slide_slidetitle_geom(Stylesheet *s, struct frame_geom geom);
extern int stylesheet_set_slide_slidetitle_font(Stylesheet *s, char *font);
extern int stylesheet_set_slide_slidetitle_pad(Stylesheet *s, double padding[4]);
extern int stylesheet_set_slide_slidetitle_paraspace(Stylesheet *s, double paraspace[4]);
extern int stylesheet_set_slide_slidetitle_fgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_slide_slidetitle_bgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_slide_slidetitle_align(Stylesheet *s, enum alignment align);

extern int stylesheet_set_narrative_font(Stylesheet *s, char *font);
extern int stylesheet_set_narrative_pad(Stylesheet *s, double padding[4]);
extern int stylesheet_set_narrative_paraspace(Stylesheet *s, double paraspace[4]);
extern int stylesheet_set_narrative_fgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_narrative_bgcol(Stylesheet *s, double rgba[4]);
extern int stylesheet_set_narrative_align(Stylesheet *s, enum alignment align);

extern const char *stylesheet_get_slide_text_font(Stylesheet *s);


#endif /* STYLESHEET_H */
