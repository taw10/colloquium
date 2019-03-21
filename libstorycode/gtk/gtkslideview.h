/*
 * sc_editor.h
 *
 * Copyright © 2014-2018 Thomas White <taw@bitwiz.org.uk>
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

#ifndef GTK_SLIDE_VIEW_H
#define GTK_SLIDE_VIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>


#include <stylesheet.h>
#include <narrative.h>
#include <presentation.h>
#include <imagestore.h>
#include <slide_render_cairo.h>


#define GTK_TYPE_SLIDE_VIEW (gtk_slide_view_get_type())

#define GTK_SLIDE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                 GTK_TYPE_SLIDE_VIEW, GtkSlideView))

#define GTK_IS_SLIDE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                    GTK_TYPE_SLIDE_VIEW))

#define GTK_SLIDE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((obj), \
                                         GTK_TYPE_SLIDE_VIEW, GtkSlideViewClass))

#define GTK_IS_SLIDE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                            GTK_TYPE_SLIDE_VIEW))

#define GTK_SLIDE_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                           GTK_TYPE_SLIDE_VIEW, GtkSlideViewClass))


enum drag_reason
{
	DRAG_REASON_NONE,
	DRAG_REASON_CREATE,
	DRAG_REASON_IMPORT,
	DRAG_REASON_RESIZE,
	DRAG_REASON_MOVE,
	DRAG_REASON_TEXTSEL
};


enum drag_corner
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


struct _gtkslideview
{
	GtkDrawingArea       parent_instance;

	/*< private >*/
	Presentation        *p;
	Slide               *slide;
	GtkIMContext        *im_context;

	int                  w;   /* Surface size in pixels */
	int                  h;

	/* Redraw/scroll stuff */
	double               view_scale;  /* The scale factor */
	double               h_scroll_pos;
	double               v_scroll_pos;
	double               visible_width;
	double               visible_height;

	/* Location of the cursor */
	SlideItem           *cursor_frame;
	struct slide_pos     cpos;

	/* Border surrounding actual slide within drawingarea */
	double               border_offs_x;
	double               border_offs_y;
	double               min_border;
	double               bgcol[3];
	GdkPixbuf           *bg_pixbuf;

	/* Rubber band boxes and related stuff */
	double               start_corner_x;
	double               start_corner_y;
	double               drag_corner_x;
	double               drag_corner_y;
	double               diagonal_length;
	double               box_x;
	double               box_y;
	double               box_width;
	double               box_height;
	enum drag_reason     drag_reason;
	enum drag_status     drag_status;
	enum drag_corner     drag_corner;
	struct slide_pos     sel_start; /* Where the user dragged from */
	struct slide_pos     sel_end;

	/* Stuff to do with drag and drop import of "content" */
	int                  drag_preview_pending;
	int                  have_drag_data;
	int                  drag_highlight;
	double               import_width;
	double               import_height;
	int                  import_acceptable;
};

struct _gtkslideviewclass
{
	GtkDrawingAreaClass parent_class;
};

typedef struct _gtkslideview GtkSlideView;
typedef struct _gtkslideviewclass GtkSlideViewClass;

extern GtkWidget *gtk_slide_view_new(Presentation *p, Slide *slide);
extern void gtk_slide_view_set_slide(GtkWidget *sv, Slide *slide);

#endif  /* GTK_SLIDE_VIEW_H */
