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


#include <gtk/gtk.h>


enum objtype
{
	RECTANGLE,
};


struct object
{
	enum objtype type;
};


struct slide
{
	cairo_surface_t *render_cache;
	int              render_cache_seq;

	int              n_objects;
	struct object   *objects;
	int              object_seq;

	double           slide_width;
	double           slide_height;
};


struct presentation
{
	char            *titlebar;
	char            *filename;

	GtkWidget       *window;
	GtkWidget	*drawingarea;
	GtkUIManager	*ui;
	GtkActionGroup	*action_group;

	double           slide_width;
	double           slide_height;
	double           border_offs_x;
	double           border_offs_y;

	/* The slide currently being displayed */
	unsigned int     view_slide;

	unsigned int     num_slides;
	struct slide   **slides;
};


extern struct presentation *new_presentation(void);


#endif	/* PRESENTATION_H */
