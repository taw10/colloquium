/*
 * presentation.h
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

#ifndef PRESENTATION_H
#define PRESENTATION_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <gtk/gtk.h>

#include "stylesheet.h"
#include "imagestore.h"

struct slide
{
	struct presentation *parent;
	struct slide_template *st;

	/* Any of these may be NULL */
	cairo_surface_t *rendered_proj;
	cairo_surface_t *rendered_edit;

	/* This should always be present (and up to date). */
	cairo_surface_t *rendered_thumb;

	struct frame    *top;

	char *notes;
};


enum drag_reason
{
	DRAG_REASON_NONE,
	DRAG_REASON_CREATE,
	DRAG_REASON_IMPORT,
	DRAG_REASON_RESIZE,
	DRAG_REASON_MOVE
};


enum corner
{
	CORNER_NONE,
	CORNER_TL,
	CORNER_TR,
	CORNER_BL,
	CORNER_BR
};


enum drag_status
{
	DRAG_STATUS_NONE,
	DRAG_STATUS_COULD_DRAG,
	DRAG_STATUS_DRAGGING,
};


/* Inelegance to make furniture selection menus work */
struct menu_pl
{
	struct presentation *p;
	struct style *sty;
	struct slide_template *st;
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
	GtkWidget       **menu_rebuild_list;
	struct menu_pl   *menu_path_list;
	int               n_menu_rebuild;
	PangoContext     *pc;
	ImageStore       *is;

	/* Pointers to the current "editing" and "projection" slides */
	struct slide     *cur_edit_slide;
	struct slide     *cur_proj_slide;
	struct slide     *cur_notes_slide;
	int               slideshow_linked;

	/* Pointers to the frame currently being edited */
	struct frame    **selection;
	int               n_selection;
	int               max_selection;
	size_t            cursor_pos;

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

	/* Rubber band boxes and related stuff */
	double            start_corner_x;
	double            start_corner_y;
	double            drag_corner_x;
	double            drag_corner_y;
	double            diagonal_length;
	double            box_x;
	double            box_y;
	double            box_width;
	double            box_height;
	enum drag_reason  drag_reason;
	enum drag_status  drag_status;
	enum corner       drag_corner;

	/* Stuff to do with drag and drop import of "content" */
	int               drag_preview_pending;
	int               have_drag_data;
	int               drag_highlight;
	double            import_width;
	double            import_height;
	int               import_acceptable;

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

extern int load_presentation(struct presentation *p, const char *filename);
extern int save_presentation(struct presentation *p, const char *filename);

extern void set_edit(struct presentation *p, struct slide *s);
extern void set_selection(struct presentation *p, struct frame *fr);
extern void add_selection(struct presentation *p, struct frame *fr);

#define UNUSED __attribute__((unused))


#endif	/* PRESENTATION_H */
