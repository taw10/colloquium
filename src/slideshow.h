/*
 * slideshow.h
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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


extern void try_start_slideshow(struct presentation *p);

extern void change_proj_slide(struct presentation *p, struct slide *np);

extern void toggle_slideshow_link(struct presentation *p);
extern void check_toggle_blank(struct presentation *p);

extern void redraw_slideshow(struct presentation *p);

extern void end_slideshow(struct presentation *p);

#endif	/* SLIDESHOW_H */
