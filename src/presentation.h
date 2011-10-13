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

#include "stylesheet.h"


struct slide
{
	struct presentation *parent;

	cairo_surface_t *render_cache;
	int              render_cache_seq;

	int              num_objects;
	struct object  **objects;
	int              object_seq;
};


enum drag_reason
{
	DRAG_REASON_NONE,
	DRAG_REASON_CREATE,
	DRAG_REASON_IMPORT,
	DRAG_REASON_TOOL,
};


enum drag_status
{
	DRAG_STATUS_NONE,
	DRAG_STATUS_COULD_DRAG,
	DRAG_STATUS_DRAGGING,
};


struct toolinfo
{
	void (*click_select)(struct presentation *p, struct toolinfo *tip,
	                     double x, double y, GdkEventButton *event,
	                     enum drag_status *drag_status,
	                     enum drag_reason *drag_reason);
	void (*create_default)(struct presentation *p, struct style *sty,
	                       struct toolinfo *tip);
	void (*select)(struct object *o, struct toolinfo *tip);
	int  (*deselect)(struct object *o, struct toolinfo *tip);
	void (*drag)(struct toolinfo *tip, struct presentation *p,
	             struct object *o, double x, double y);
	void (*end_drag)(struct toolinfo *tip, struct presentation *p,
	                 struct object *o, double x, double y);

	void (*create_region)(struct toolinfo *tip, struct presentation *p,
	                      double x1, double y1, double x2, double y2);

	void (*draw_editing_overlay)(struct toolinfo *tip, cairo_t *cr,
	                             struct object *o);
	void (*key_pressed)(struct object *o, guint keyval,
	                    struct toolinfo *tip);
	void (*im_commit)(struct object *o, gchar *str, struct toolinfo *tip);
};


struct presentation
{
	char             *titlebar;
	char             *filename;
	int               completely_empty;

	struct toolinfo  *select_tool;
	struct toolinfo  *text_tool;

	GtkWidget        *window;
	GtkWidget        *drawingarea;
	GtkUIManager     *ui;
	GtkActionGroup   *action_group;
	GtkIMContext     *im_context;
	GtkWidget        *tbox;

	/* Stylesheet */
	StyleSheet       *ss;
	struct style     *default_style;

	/* Dialogue boxes */
	StylesheetWindow *stylesheetwindow;

	/* Slideshow stuff */
	GtkWidget        *slideshow;
	GtkWidget        *ss_drawingarea;
	GdkCursor        *blank_cursor;
	int               ss_blank;
	char              ss_geom[256];

	double            slide_width;
	double            slide_height;
	double            border_offs_x;
	double            border_offs_y;

	/* The slide currently being displayed */
	unsigned int      view_slide_number;
	struct slide     *view_slide;
	struct object    *editing_object;

	/* Tool status */
	struct toolinfo  *cur_tool;

	/* Rubber band boxes and related stuff */
	double            start_corner_x;
	double            start_corner_y;
	double            drag_corner_x;
	double            drag_corner_y;
	enum drag_reason  drag_reason;
	enum drag_status  drag_status;

	/* Stuff to do with drag and drop import of "content" */
	int               drag_preview_pending;
	int               have_drag_data;
	int               drag_highlight;
	double            import_width;
	double            import_height;
	int               import_acceptable;

	unsigned int      num_slides;
	struct slide    **slides;
};


extern struct presentation *new_presentation(void);
extern struct slide *add_slide(struct presentation *p, int pos);
extern int add_object_to_slide(struct slide *s, struct object *o);
extern void remove_object_from_slide(struct slide *s, struct object *o);
extern struct object *find_object_at_position(struct slide *s,
                                              double x, double y);

#endif	/* PRESENTATION_H */
