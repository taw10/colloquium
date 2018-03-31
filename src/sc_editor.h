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

#ifndef SC_EDITOR_H
#define SC_EDITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

#include "frame.h"
#include "sc_interp.h"

struct presentation;


#define SC_TYPE_EDITOR            (sc_editor_get_type())

#define SC_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                   SC_TYPE_EDITOR, SCEditor))

#define SC_IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                   SC_TYPE_EDITOR))

#define SC_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((obj), \
                                   SC_TYPE_EDITOR, SCEditorClass))

#define SC_IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                   SC_TYPE_EDITOR))

#define SC_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                   SC_TYPE_EDITOR, SCEditorClass))

enum drag_reason
{
	DRAG_REASON_NONE,
	DRAG_REASON_CREATE,
	DRAG_REASON_IMPORT,
	DRAG_REASON_RESIZE,
	DRAG_REASON_MOVE,
	DRAG_REASON_TEXTSEL
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


struct _sceditor
{
	GtkDrawingArea       parent_instance;
	PangoLanguage        *lang;

	/*< private >*/
	GtkIMContext        *im_context;
	int                  w;   /* Surface size in pixels */
	int                  h;
	double               log_w;  /* Size of surface in "SC units" */
	double               log_h;
	SCBlock             *scblocks;
	SCBlock             **stylesheets;
	ImageStore          *is;
	SCCallbackList      *cbl;
	struct frame        *top;
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
	int                  flow;
	int                  scale;       /* Whether the SCEditor should scale to fit */
	double               view_scale;  /* The scale factor, if scale=1 */

	/* Pointers to the frame currently being edited */
	struct frame        *selection;
	int                 top_editable;

	PangoContext        *pc;

	/* Location of the cursor */
	struct frame        *cursor_frame;
	struct edit_pos      cpos;

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
	enum corner          drag_corner;
	int                  sel_active;
	struct edit_pos      sel_start; /* Where the user dragged from */
	struct edit_pos      sel_end;

	/* Stuff to do with drag and drop import of "content" */
	int                  drag_preview_pending;
	int                  have_drag_data;
	int                  drag_highlight;
	double               import_width;
	double               import_height;
	int                  import_acceptable;

	/* Stuff that doesn't really belong here */
	int                  slidenum;
};

struct _sceditorclass
{
	GtkDrawingAreaClass parent_class;
};

typedef struct _sceditor SCEditor;
typedef struct _sceditorclass SCEditorClass;

extern void sc_editor_set_scblock(SCEditor *e, SCBlock *scblocks);
extern void sc_editor_set_stylesheets(SCEditor *e, SCBlock **stylesheets);
extern SCBlock *sc_editor_get_scblock(SCEditor *e);
extern GtkWidget *sc_editor_get_widget(SCEditor *e);
extern SCEditor *sc_editor_new(SCBlock *scblocks, SCBlock **stylesheets,
                               PangoLanguage *lang, const char *storename);
extern void sc_editor_set_size(SCEditor *e, int w, int h);
extern void sc_editor_set_logical_size(SCEditor *e, double w, double h);
extern void sc_editor_set_flow(SCEditor *e, int flow);
extern void sc_editor_set_scale(SCEditor *e, int scale);
extern void sc_editor_redraw(SCEditor *e);
extern void sc_editor_set_background(SCEditor *e, double r, double g, double b);
extern void sc_editor_set_slidenum(SCEditor *e, int slidenum);
extern void sc_editor_set_min_border(SCEditor *e, double min_border);
extern void sc_editor_set_top_frame_editable(SCEditor *e,
                                             int top_frame_editable);
extern void sc_editor_set_callbacks(SCEditor *e, SCCallbackList *cbl);
extern void sc_editor_paste(SCEditor *e);
extern void sc_editor_add_storycode(SCEditor *e, const char *sc);
extern void sc_editor_copy_selected_frame(SCEditor *e);
extern void sc_editor_delete_selected_frame(SCEditor *e);
extern void sc_editor_remove_cursor(SCEditor *e);
extern void sc_editor_ensure_cursor(SCEditor *e);
extern SCBlock *split_paragraph_at_cursor(SCEditor *e);

extern void sc_editor_set_imagestore(SCEditor *e, ImageStore *is);
extern void sc_editor_set_para_highlight(SCEditor *e, int para_highlight);
extern int sc_editor_get_cursor_para(SCEditor *e);
extern void *sc_editor_get_cursor_bvp(SCEditor *e);
extern void sc_editor_set_cursor_para(SCEditor *e, signed int pos);
extern int sc_editor_get_num_paras(SCEditor *e);

#endif	/* SC_EDITOR_H */
