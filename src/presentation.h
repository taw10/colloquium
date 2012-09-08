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
#include <pango/pango.h>
#include <gtk/gtk.h>

struct frame;
#include "layout.h"
#include "stylesheet.h"

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
	PangoContext *pc;  /* FIXME: Doesn't belong here */

	struct frame            **rendering_order;
	int                       num_ro;
	int                       max_ro;

	char                     *sc;  /* Storycode */

	struct layout_parameters  lop;
	struct style             *style;  /* Non-NULL if 'lop' came from SS */

	/* Location relative to parent, calculated from alignment parameters */
	double                    offs_x;
	double                    offs_y;
	double                    w;
	double                    h;

	/* True if this frame should be deleted on the next mouse click */
	int                       empty;
};


struct presentation
{
	char             *titlebar;
	char             *filename;
	int               completely_empty;
	int              *num_presentations;

	GtkWidget        *window;
	GtkWidget        *drawingarea;
	GtkUIManager     *ui;
	GtkActionGroup   *action_group;
	GtkIMContext     *im_context;

	/* Pointers to the current "editing" and "projection" slides */
	struct slide     *cur_edit_slide;
	struct slide     *cur_proj_slide;
	struct slide     *cur_notes_slide;
	int               slideshow_linked;

	/* This is the "native" size of the slide.  It only exists to give
	 * font size some meaning in the context of a somewhat arbitrary DPI */
	double            slide_width;
	double            slide_height;

	/* Width of a slide in the editor, projector or thumbnail (pixels) */
	int               edit_slide_width;
	int               proj_slide_width;
	int               thumb_slide_width;

	/* This is just to help with rendering the slides within the
	 * editing window. */
	double            border_offs_x;
	double            border_offs_y;

	/* Slideshow stuff */
	GtkWidget        *slideshow;
	GtkWidget        *ss_drawingarea;
	GdkCursor        *blank_cursor;
	int               ss_blank;
	char              ss_geom[256];

	StyleSheet       *ss;
	unsigned int      num_slides;
	struct slide    **slides;
};


extern struct presentation *new_presentation(void);
extern void free_presentation(struct presentation *p);

extern struct slide *new_slide(void);
extern struct slide *add_slide(struct presentation *p, int pos);
extern int insert_slide(struct presentation *p, struct slide *s, int pos);
extern void free_slide(struct slide *s);

extern void get_titlebar_string(struct presentation *p);

extern int slide_number(struct presentation *p, struct slide *s);

#define UNUSED __attribute__((unused))


#endif	/* PRESENTATION_H */
