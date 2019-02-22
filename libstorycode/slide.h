/*
 * slide.h
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

#ifndef SLIDE_H
#define SLIDE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _slide Slide;
typedef struct _slideitem SlideItem;

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


extern Slide *slide_new(void);
extern void slide_free(Slide *s);

extern int slide_add_prestitle(Slide *s, char *prestitle);
extern int slide_add_image(Slide *s, char *filename, struct frame_geom geom);
extern int slide_add_text(Slide *s, char **text, int n_text, struct frame_geom geom);
extern int slide_add_footer(Slide *s);
extern int slide_add_slidetitle(Slide *s, char *slidetitle);
extern int slide_set_logical_size(Slide *s, double w, double h);

extern int slide_get_logical_size(Slide *s, double *w, double *h);

/* For debugging, not really part of API */
extern void describe_slide(Slide *s);

#endif /* SLIDE_H */
