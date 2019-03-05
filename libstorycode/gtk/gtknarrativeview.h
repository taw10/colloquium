/*
 * gtknarrativeview.h
 *
 * Copyright © 2014-2019 Thomas White <taw@bitwiz.org.uk>
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

#ifndef GTK_NARRATIVE_VIEW_H
#define GTK_NARRATIVE_VIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

#include <stylesheet.h>
#include <narrative.h>
#include <presentation.h>
#include <imagestore.h>

#define GTK_TYPE_NARRATIVE_VIEW (gtk_narrative_view_get_type())

#define GTK_NARRATIVE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                 GTK_TYPE_NARRATIVE_VIEW, GtkNarrativeView))

#define GTK_IS_NARRATIVE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                    GTK_TYPE_NARRATIVE_VIEW))

#define GTK_NARRATIVE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((obj), \
                                         GTK_TYPE_NARRATIVE_VIEW, GtkNarrativeViewClass))

#define GTK_IS_NARRATIVE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                            GTK_TYPE_NARRATIVE_VIEW))

#define GTK_NARRATIVE_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                           GTK_TYPE_NARRATIVE_VIEW, GtkNarrativeViewClass))

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


struct edit_pos
{
	int para;    /* Paragraph number (corresponding to narrative items) */
	int pos;     /* Byte position within paragraph
	              * Yes, really.  See pango_layout_xy_to_index */
	int trail;
};


struct _gtknarrativeview
{
	GtkDrawingArea       parent_instance;

	/*< private >*/
	Presentation        *p;
	GtkIMContext        *im_context;

	int                  w;   /* Surface size in pixels */
	int                  h;
	int                  para_highlight;

	/* Redraw/scroll stuff */
	GtkScrollablePolicy  hpol;
	GtkScrollablePolicy  vpol;
	GtkAdjustment       *hadj;
	GtkAdjustment       *vadj;
	double               scroll_pos;
	double               h_scroll_pos;
	int                  visible_height;
	int                  visible_width;

	/* Location of the cursor */
	struct edit_pos      cpos;

	/* Rubber band boxes and related stuff */
	enum drag_status     drag_status;
	int                  sel_active;
	struct edit_pos      sel_start; /* Where the user dragged from */
	struct edit_pos      sel_end;
};

struct _gtknarrativeviewclass
{
	GtkDrawingAreaClass parent_class;
};

typedef struct _gtknarrativeview GtkNarrativeView;
typedef struct _gtknarrativeviewclass GtkNarrativeViewClass;

extern GtkNarrativeView *gtk_narrative_view_new(Presentation *p);

extern void gtk_narrative_view_set_logical_size(GtkNarrativeView *e, double w, double h);

extern void gtk_narrative_view_paste(GtkNarrativeView *e);

extern void gtk_narrative_view_set_para_highlight(GtkNarrativeView *e, int para_highlight);
extern int gtk_narrative_view_get_cursor_para(GtkNarrativeView *e);
extern void gtk_narrative_view_set_cursor_para(GtkNarrativeView *e, signed int pos);

#endif	/* GTK_NARRATIVE_VIEW_H */
