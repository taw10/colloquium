/*
 * presentation.h
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

#ifndef PRESENTATION_H
#define PRESENTATION_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>

struct slide
{
	struct presentation *parent;
	struct slide_template *st;

	/* Any of these may be NULL */
	cairo_surface_t *rendered_proj;
	cairo_surface_t *rendered_edit;

	/* This should always be present (and up to date). */
	cairo_surface_t *rendered_thumb;

	struct frame   *top;

	char *notes;
};


struct frame
{
	struct frame_class *cl;

	struct frame      **rendering_order;
	int                 num_ro;

	char               *sc;  /* Storycode */

	int                 empty;
};


#endif	/* PRESENTATION_H */
