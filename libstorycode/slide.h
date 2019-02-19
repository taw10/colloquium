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

struct frame_geom
{
	double x;
	double y;
	double w;
	double h;
	/* FIXME: units */
};


extern Slide *slide_new(void);
extern void slide_free(Slide *s);

extern int slide_add_prestitle(Slide *s, char *prestitle);
extern int slide_add_image(Slide *s, char *filename, struct frame_geom geom);
extern int slide_add_text(Slide *s, char *text, struct frame_geom geom);
extern int slide_add_footer(Slide *s);
extern int slide_add_slidetitle(Slide *s, char *slidetitle);


#endif /* SLIDE_H */
