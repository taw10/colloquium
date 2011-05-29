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


struct slide
{
	cairo_surface_t *render_cache;
	int              render_cache_seq;

	int              num_objects;
	struct object  **objects;
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
	GtkIMContext    *im_context;

	/* Slideshow stuff */
	GtkWidget       *slideshow;
	GtkWidget       *ss_drawingarea;
	GdkCursor       *blank_cursor;
	int              ss_blank;

	double           slide_width;
	double           slide_height;
	double           border_offs_x;
	double           border_offs_y;

	/* The slide currently being displayed */
	unsigned int     view_slide_number;
	struct slide    *view_slide;
	struct object   *editing_object;

	unsigned int     num_slides;
	struct slide   **slides;
};


extern struct presentation *new_presentation(void);
extern struct slide *add_slide(struct presentation *p, int pos);
extern int add_object_to_slide(struct slide *s, struct object *o);
extern void remove_object_from_slide(struct slide *s, struct object *o);
extern struct object *find_object_at_position(struct slide *s,
                                              double x, double y);

#endif	/* PRESENTATION_H */
