/*
 * slideshow.h
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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

#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "slide_window.h"

/* Opaque data structure representing a slideshow */
typedef struct _slideshow SlideShow;

extern SlideShow *try_start_slideshow(SlideWindow *sw, struct presentation *p);
extern void end_slideshow(SlideShow *ss);

extern void change_proj_slide(SlideShow *ss, struct slide *np);
extern struct slide *slideshow_slide(SlideShow *ss);

extern void toggle_slideshow_link(SlideShow *ss);
extern int slideshow_linked(SlideShow *ss);
extern void check_toggle_blank(SlideShow *ss);

extern void redraw_slideshow(SlideShow *ss);
extern void slideshow_rerender(SlideShow *ss);

#endif	/* SLIDESHOW_H */
