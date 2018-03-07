/*
 * sc_editor.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <assert.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>

#include "colloquium.h"
#include "presentation.h"
#include "slide_window.h"
#include "render.h"
#include "frame.h"
#include "sc_parse.h"
#include "sc_interp.h"
#include "sc_editor.h"
#include "slideshow.h"
#include "debugger.h"


static void scroll_interface_init(GtkScrollable *iface)
{
}


enum
{
	SCEDITOR_0,
	SCEDITOR_VADJ,
	SCEDITOR_HADJ,
	SCEDITOR_VPOL,
	SCEDITOR_HPOL,
};


G_DEFINE_TYPE_WITH_CODE(SCEditor, sc_editor, GTK_TYPE_DRAWING_AREA,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE,
                                              scroll_interface_init))

static void debug_paragraphs(SCEditor *e)
{
	struct frame *fr = e->cursor_frame;
	int i;

	printf("Paragraphs in current frame:\n");
	for ( i=0; i<fr->n_paras; i++ ) {
		show_para(fr->paras[i]);
	}
}


static void show_cursor_pos(SCEditor *e)
{
	fprintf(stderr, "Cursor: fr %p, para %i, pos %li, trail %i\n",
	        e->cursor_frame, e->cursor_para, (long int)e->cursor_pos,
		e->cursor_trail);
}



static void horizontal_adjust(GtkAdjustment *adj, SCEditor *e)
{
	e->h_scroll_pos = gtk_adjustment_get_value(adj);
	sc_editor_redraw(e);
}


static void set_horizontal_params(SCEditor *e)
{
	if ( e->hadj == NULL ) return;
	gtk_adjustment_configure(e->hadj, e->h_scroll_pos, 0, e->w, 100,
	                         e->visible_width, e->visible_width);
}


static void vertical_adjust(GtkAdjustment *adj, SCEditor *e)
{
	e->scroll_pos = gtk_adjustment_get_value(adj);
	sc_editor_redraw(e);
}


static void set_vertical_params(SCEditor *e)
{
	double page;

	if ( e->vadj == NULL ) return;

	/* Ensure we do not scroll off the top of the document */
	if ( e->scroll_pos < 0.0 ) e->scroll_pos = 0.0;

	/* Ensure we do not scroll off the bottom of the document */
	if ( e->scroll_pos > e->h - e->visible_height ) {
		e->scroll_pos = e->h - e->visible_height;
	}

	/* If we can show the whole document, show it at the top */
	if ( e->h < e->visible_height ) {
		e->scroll_pos = 0.0;
	}

	if ( e->h > e->visible_height ) {
		page = e->visible_height;
	} else {
		page = e->h;
	}

	gtk_adjustment_configure(e->vadj, e->scroll_pos, 0, e->h, 100,
	                         e->visible_height, page);
}


static void update_size(SCEditor *e)
{
	if ( e->flow ) {

		double total = total_height(e->top);

		e->w = e->top->w;
		e->h = total + e->top->pad_t + e->top->pad_b;

		e->log_w = e->w;
		e->log_h = e->h;
		e->top->h = e->h;
	} else {
		e->top->w = e->log_w;
		e->top->h = e->log_h;
	}

	if ( e->flow && (e->top->h < e->visible_height) ) {
		e->top->h = e->visible_height;
	}

	set_vertical_params(e);
	set_horizontal_params(e);
}


static gboolean resize_sig(GtkWidget *widget, GdkEventConfigure *event,
                           SCEditor *e)
{
	PangoContext *pc;

	pc = gdk_pango_context_get();

	if ( e->scale ) {

		double sx, sy;
		double aw, ah;

		e->w = event->width;
		e->h = event->height;
		sx = (double)e->w / e->log_w;
		sy = (double)e->h / e->log_h;
		e->view_scale = (sx < sy) ? sx : sy;

		/* Actual size (in device units) */
		aw = e->view_scale * e->log_w;
		ah = e->view_scale * e->log_h;

		e->border_offs_x = (event->width - aw)/2.0;
		e->border_offs_y = (event->height - ah)/2.0;

	}

	e->visible_height = event->height;
	e->visible_width = event->width;

	/* Interpret and shape, if not already done */
	if ( e->top == NULL ) {
		double w, h;
		if ( e->flow ) {
			w = event->width;
			h = 0.0;
		} else {
			w = e->log_w;
			h = e->log_h;
		}
		e->top = interp_and_shape(sc_block_child(e->scblocks),
		                          e->stylesheets, e->cbl,
		                          e->is, e->slidenum, pc,
		                          w, h, e->lang);
		recursive_wrap(e->top, pc);
	}

	if ( e->flow ) {
		/* Wrap using current width */
		e->top->w = event->width;
		e->top->h = 0.0;  /* To be updated in a moment */
		e->top->x = 0.0;
		e->top->y = 0.0;
		/* Only the top level needs to be wrapped */
		wrap_frame(e->top, pc);
	}

	update_size(e);

	g_object_unref(pc);

	return FALSE;
}


static void emit_change_sig(SCEditor *e)
{
	g_signal_emit_by_name(e, "changed");
}


void sc_editor_set_flow(SCEditor *e, int flow)
{
	e->flow = flow;
}


static void sc_editor_set_property(GObject *obj, guint id, const GValue *val,
                                   GParamSpec *spec)
{
	SCEditor *e = SC_EDITOR(obj);

	switch ( id ) {

		case SCEDITOR_VPOL :
		e->vpol = g_value_get_enum(val);
		break;

		case SCEDITOR_HPOL :
		e->hpol = g_value_get_enum(val);
		break;

		case SCEDITOR_VADJ :
		e->vadj = g_value_get_object(val);
		set_vertical_params(e);
		if ( e->vadj != NULL ) {
			g_signal_connect(G_OBJECT(e->vadj), "value-changed",
			                 G_CALLBACK(vertical_adjust), e);
		}
		break;

		case SCEDITOR_HADJ :
		e->hadj = g_value_get_object(val);
		set_horizontal_params(e);
		if ( e->hadj != NULL ) {
			g_signal_connect(G_OBJECT(e->hadj), "value-changed",
			                 G_CALLBACK(horizontal_adjust), e);
		}
		break;

		default :
		printf("setting %i\n", id);
		break;

	}
}


static void sc_editor_get_property(GObject *obj, guint id, GValue *val,
                                   GParamSpec *spec)
{
	SCEditor *e = SC_EDITOR(obj);

	switch ( id ) {

		case SCEDITOR_VADJ :
		g_value_set_object(val, e->vadj);
		break;

		case SCEDITOR_HADJ :
		g_value_set_object(val, e->hadj);
		break;

		case SCEDITOR_VPOL :
		g_value_set_enum(val, e->vpol);
		break;

		case SCEDITOR_HPOL :
		g_value_set_enum(val, e->hpol);
		break;

		default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;

	}
}


static GtkSizeRequestMode get_request_mode(GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}


static void get_preferred_width(GtkWidget *widget, gint *min, gint *natural)
{
	SCEditor *e = SC_EDITOR(widget);
	if ( e->flow ) {
		*min = 100;
		*natural = 640;
	} else {
		*min = e->w;
		*natural = e->w;
	}
}


static void get_preferred_height(GtkWidget *widget, gint *min, gint *natural)
{
	SCEditor *e = SC_EDITOR(widget);
	if ( e->flow ) {
		*min = 1000;
		*natural = 1000;
	} else {
		*min = e->h;
		*natural = e->h;
	}
}


static void sc_editor_class_init(SCEditorClass *klass)
{
	GObjectClass *goc = G_OBJECT_CLASS(klass);
	goc->set_property = sc_editor_set_property;
	goc->get_property = sc_editor_get_property;
	g_object_class_override_property(goc, SCEDITOR_VADJ, "vadjustment");
	g_object_class_override_property(goc, SCEDITOR_HADJ, "hadjustment");
	g_object_class_override_property(goc, SCEDITOR_VPOL, "vscroll-policy");
	g_object_class_override_property(goc, SCEDITOR_HPOL, "hscroll-policy");

	GTK_WIDGET_CLASS(klass)->get_request_mode = get_request_mode;
	GTK_WIDGET_CLASS(klass)->get_preferred_width = get_preferred_width;
	GTK_WIDGET_CLASS(klass)->get_preferred_height = get_preferred_height;
	GTK_WIDGET_CLASS(klass)->get_preferred_height_for_width = NULL;

	g_signal_new("changed", SC_TYPE_EDITOR, G_SIGNAL_RUN_LAST, 0,
	             NULL, NULL, NULL, G_TYPE_NONE, 0);
}


static void sc_editor_init(SCEditor *e)
{
	e->vpol = GTK_SCROLL_NATURAL;
	e->hpol = GTK_SCROLL_NATURAL;
	e->vadj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
	e->hadj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
}


void sc_editor_set_background(SCEditor *e, double r, double g, double b)
{
	e->bgcol[0] = r;
	e->bgcol[1] = g;
	e->bgcol[2] = b;
}


void sc_editor_remove_cursor(SCEditor *e)
{
	e->cursor_frame = NULL;
	e->cursor_para = 0;
	e->cursor_pos = 0;
	e->cursor_trail = 0;
	e->selection = NULL;
}


/* (Re-)run the entire rendering pipeline.
 * NB "full" means "full".  All frame, line and box handles will become
 * invalid.  The cursor position will be unset. */
static void full_rerender(SCEditor *e)
{
	PangoContext *pc;

	frame_free(e->top);
	sc_editor_remove_cursor(e);

	pc = gdk_pango_context_get();

	e->top = interp_and_shape(sc_block_child(e->scblocks),
	                          e->stylesheets, e->cbl,
	                          e->is, e->slidenum,
	                          pc, e->log_w, 0.0, e->lang);

	e->top->x = 0.0;
	e->top->y = 0.0;
	e->top->w = e->w;
	e->top->h = 0.0;  /* To be updated in a moment */

	recursive_wrap(e->top, pc);
	update_size(e);

	sc_editor_redraw(e);

	g_object_unref(pc);
}


void sc_editor_redraw(SCEditor *e)
{
	gint w, h;

	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));

	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


void paste_received(GtkClipboard *cb, const gchar *t, gpointer vp)
{
	SCEditor *e = vp;
	SCBlock *nf;
	nf = sc_parse(t);
	sc_block_append_block(e->scblocks, nf);
	full_rerender(e);
}


void sc_editor_paste(SCEditor *e)
{
	GtkClipboard *cb;
	GdkAtom atom;

	atom = gdk_atom_intern("CLIPBOARD", FALSE);
	if ( atom == GDK_NONE ) return;
	cb = gtk_clipboard_get(atom);
	gtk_clipboard_request_text(cb, paste_received, e);
}


void sc_editor_add_storycode(SCEditor *e, const char *sc)
{
	SCBlock *nf;
	nf = sc_parse(sc);
	sc_block_append_block(sc_block_child(e->scblocks), nf);
	full_rerender(e);
}


void sc_editor_copy_selected_frame(SCEditor *e)
{
	char *t;
	GtkClipboard *cb;
	GdkAtom atom;

	if ( e->selection == NULL ) return;

	atom = gdk_atom_intern("CLIPBOARD", FALSE);
	if ( atom == GDK_NONE ) return;

	cb = gtk_clipboard_get(atom);

	t = serialise_sc_block(e->selection->scblocks);

	gtk_clipboard_set_text(cb, t, -1);
	free(t);
}


void sc_editor_delete_selected_frame(SCEditor *e)
{
	SCBlock *scb_old = e->scblocks;
	sc_block_delete(&e->scblocks, e->selection->scblocks);
	assert(scb_old == e->scblocks);
	full_rerender(e);
	emit_change_sig(e);
}


static gint destroy_sig(GtkWidget *window, SCEditor *e)
{
	return 0;
}


static void draw_editing_box(cairo_t *cr, struct frame *fr)
{
	const double dash[] = {2.0, 2.0};
	double xmin, ymin, width, height;
	double ptot_w, ptot_h;

	xmin = fr->x;
	ymin = fr->y;
	width = fr->w;
	height = fr->h;

	cairo_new_path(cr);
	cairo_rectangle(cr, xmin, ymin, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.69, 1.0);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);

	cairo_new_path(cr);
	ptot_w = fr->pad_l + fr->pad_r;
	ptot_h = fr->pad_t + fr->pad_b;
	cairo_rectangle(cr, xmin+fr->pad_l, ymin+fr->pad_t,
	                    width-ptot_w, height-ptot_h);
	cairo_set_dash(cr, dash, 2, 0.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.1);
	cairo_stroke(cr);

	cairo_set_dash(cr, NULL, 0, 0.0);
}


static void draw_para_highlight(cairo_t *cr, struct frame *fr, int cursor_para)
{
	double cx, cy, w, h;

	if ( get_para_highlight(fr, cursor_para, &cx, &cy, &w, &h) != 0 ) {
		return;
	}

	cairo_new_path(cr);
	cairo_rectangle(cr, cx+fr->x, cy+fr->y, w, h);
	cairo_set_source_rgba(cr, 0.7, 0.7, 1.0, 0.5);
	cairo_set_line_width(cr, 5.0);
	cairo_stroke(cr);
}


static void draw_caret(cairo_t *cr, struct frame *fr, int cursor_para,
                       size_t cursor_pos, int cursor_trail, int hgh)
{
	double cx, clow, chigh, h;
	const double t = 1.8;
	size_t offs;
	Paragraph *para;

	if ( hgh ) {
		draw_para_highlight(cr, fr, cursor_para);
		return;
	}

	assert(fr != NULL);

	para = fr->paras[cursor_para];
	if ( para_type(para) != PARA_TYPE_TEXT ) {
		draw_para_highlight(cr, fr, cursor_para);
		return;
	}

	offs = pos_trail_to_offset(para, cursor_pos, cursor_trail);
	get_cursor_pos(fr, cursor_para, offs, &cx, &clow, &h);

	cx += fr->x;
	clow += fr->y;
	chigh = clow + h;

	cairo_move_to(cr, cx, clow);
	cairo_line_to(cr, cx, chigh);

	cairo_move_to(cr, cx-t, clow-t);
	cairo_line_to(cr, cx, clow);
	cairo_move_to(cr, cx+t, clow-t);
	cairo_line_to(cr, cx, clow);

	cairo_move_to(cr, cx-t, chigh+t);
	cairo_line_to(cr, cx, chigh);
	cairo_move_to(cr, cx+t, chigh+t);
	cairo_line_to(cr, cx, chigh);

	cairo_set_source_rgb(cr, 0.86, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
}


static void draw_resize_handle(cairo_t *cr, double x, double y)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, 20.0, 20.0);
	cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
	cairo_fill(cr);
}


static void draw_overlay(cairo_t *cr, SCEditor *e)
{
	double x, y, w, h;

	if ( e->selection != NULL ) {

		draw_editing_box(cr, e->selection);

		x = e->selection->x;
		y = e->selection->y;
		w = e->selection->w;
		h = e->selection->h;

		if ( e->selection->resizable ) {
			/* Draw resize handles */
			draw_resize_handle(cr, x, y+h-20.0);
			draw_resize_handle(cr, x+w-20.0, y);
			draw_resize_handle(cr, x, y);
			draw_resize_handle(cr, x+w-20.0, y+h-20.0);
		}

		draw_caret(cr, e->cursor_frame, e->cursor_para, e->cursor_pos,
		           e->cursor_trail, e->para_highlight);

	}

	if ( (e->drag_status == DRAG_STATUS_DRAGGING)
	  && ((e->drag_reason == DRAG_REASON_CREATE)
	      || (e->drag_reason == DRAG_REASON_IMPORT)) )
	{
		cairo_new_path(cr);
		cairo_rectangle(cr, e->start_corner_x, e->start_corner_y,
		                    e->drag_corner_x - e->start_corner_x,
		                    e->drag_corner_y - e->start_corner_y);
		cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
		cairo_set_line_width(cr, 0.5);
		cairo_stroke(cr);
	}

	if ( (e->drag_status == DRAG_STATUS_DRAGGING)
	  && ((e->drag_reason == DRAG_REASON_RESIZE)
	   || (e->drag_reason == DRAG_REASON_MOVE)) )
	{
		cairo_new_path(cr);
		cairo_rectangle(cr, e->box_x, e->box_y,
		                    e->box_width, e->box_height);
		cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
		cairo_set_line_width(cr, 0.5);
		cairo_stroke(cr);
	}
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, SCEditor *e)
{
	/* Ultimate background */
	gdk_cairo_set_source_pixbuf(cr, e->bg_pixbuf, 0.0, 0.0);
	cairo_pattern_t *patt = cairo_get_source(cr);
	cairo_pattern_set_extend(patt, CAIRO_EXTEND_REPEAT);
	cairo_paint(cr);

	cairo_translate(cr, e->border_offs_x, e->border_offs_y);
	cairo_translate(cr, -e->h_scroll_pos, -e->scroll_pos);
	cairo_scale(cr, e->view_scale, e->view_scale);

	/* Rendering background */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, e->log_w, e->log_h);
	cairo_fill(cr);

	/* Contents */
	recursive_draw(e->top, cr, e->is,
	               e->scroll_pos/e->view_scale,
	               (e->scroll_pos + e->visible_height)/e->view_scale);

	/* Editing overlay */
	draw_overlay(cr, e);

	return FALSE;
}


SCBlock *split_paragraph_at_cursor(SCEditor *e)
{
	size_t offs;
	Paragraph *para;

	if ( e->cursor_frame == NULL ) return NULL;

	para = e->cursor_frame->paras[e->cursor_para];
	offs = pos_trail_to_offset(para, e->cursor_pos, e->cursor_trail);
	return split_paragraph(e->cursor_frame, e->cursor_para, offs, e->pc);
}


static void check_cursor_visible(SCEditor *e)
{
	double x, y, h;
	size_t offs;
	Paragraph *para;

	if ( e->cursor_frame == NULL ) return;

	para = e->cursor_frame->paras[e->cursor_para];
	offs = pos_trail_to_offset(para, e->cursor_pos, e->cursor_trail);
	get_cursor_pos(e->cursor_frame, e->cursor_para, offs, &x, &y, &h);

	/* Off the bottom? */
	if ( y - e->scroll_pos + h > e->visible_height ) {
		e->scroll_pos = y + h - e->visible_height;
		e->scroll_pos += e->cursor_frame->pad_b;
	}

	/* Off the top? */
	if ( y < e->scroll_pos ) {
		e->scroll_pos = y - e->cursor_frame->pad_t;
	}
}


static void do_backspace(struct frame *fr, SCEditor *e)
{
	double wrapw = e->cursor_frame->w - e->cursor_frame->pad_l - e->cursor_frame->pad_r;

	if ( e->sel_active ) {

		/* Delete the selected block */
		delete_text_from_frame(e->cursor_frame, e->sel_start, e->sel_end, wrapw);

		/* Cursor goes at start of deletion */
		sort_positions(&e->sel_start, &e->sel_end);
		e->cursor_para = e->sel_start.para;
		e->cursor_pos = e->sel_start.pos;
		e->cursor_trail = e->sel_start.trail;
		e->sel_active = 0;

	} else {

		/* Delete one character */
		struct edit_pos p1, p2;

		p1.para = e->cursor_para;
		p1.pos = e->cursor_pos;
		p1.trail = e->cursor_trail;

		p2 = p1;

		cursor_moveh(e->cursor_frame, &p2.para, &p2.pos, &p2.trail, -1);
		show_edit_pos(p1);
		show_edit_pos(p2);

		delete_text_from_frame(e->cursor_frame, p1, p2, wrapw);

		e->cursor_para = p2.para;
		e->cursor_pos = p2.pos;
		e->cursor_trail = p2.trail;

	}

	emit_change_sig(e);
	sc_editor_redraw(e);
}


static void insert_text(char *t, SCEditor *e)
{
	Paragraph *para;

	if ( e->cursor_frame == NULL ) {
		fprintf(stderr, "Inserting text into no frame.\n");
	        return;
	}

	if ( e->cursor_para >= e->cursor_frame->n_paras ) {
		fprintf(stderr, "Cursor paragraph number is too high!\n");
		return;
	}

	if ( e->sel_active ) {
		do_backspace(e->cursor_frame, e);
	}

	if ( strcmp(t, "\n") == 0 ) {
		split_paragraph_at_cursor(e);
		if ( e->flow ) update_size(e);
		cursor_moveh(e->cursor_frame, &e->cursor_para,
		             &e->cursor_pos, &e->cursor_trail, +1);
		check_cursor_visible(e);
		emit_change_sig(e);
		sc_editor_redraw(e);
		return;
	}

	para = e->cursor_frame->paras[e->cursor_para];

	/* Is this paragraph even a text one? */
	if ( para_type(para) == PARA_TYPE_TEXT ) {

		size_t off;

		/* Yes. The "easy" case */
		off = pos_trail_to_offset(para, e->cursor_pos, e->cursor_trail);
		insert_text_in_paragraph(para, off, t);
		wrap_paragraph(para, NULL,
		               e->cursor_frame->w - e->cursor_frame->pad_l
		                            - e->cursor_frame->pad_r, 0, 0);
		if ( e->flow ) update_size(e);

		cursor_moveh(e->cursor_frame, &e->cursor_para,
		             &e->cursor_pos, &e->cursor_trail, +1);

	} else {

		SCBlock *bd;
		SCBlock *ad;
		Paragraph *pnew;

		bd = para_scblock(para);
		if ( bd == NULL ) {
			fprintf(stderr, "No SCBlock for para\n");
			return;
		}

		/* No. Create a new text paragraph straight afterwards */
		ad = sc_block_insert_after(bd, NULL, NULL, strdup(t));
		if ( ad == NULL ) {
			fprintf(stderr, "Failed to add SCBlock\n");
			return;
		}

		pnew = insert_paragraph(e->cursor_frame, e->cursor_para);
		if ( pnew == NULL ) {
			fprintf(stderr, "Failed to insert paragraph\n");
			return;
		}
		add_run(pnew, ad, ad, e->cursor_frame->fontdesc,
		        e->cursor_frame->col);

		wrap_frame(e->cursor_frame, e->pc);

		e->cursor_para += 1;
		e->cursor_pos = 0;
		e->cursor_trail = 1;

	}

	emit_change_sig(e);
	check_cursor_visible(e);
	sc_editor_redraw(e);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              SCEditor *e)
{
	insert_text(str, e);
	return FALSE;
}


static int within_frame(struct frame *fr, double x, double y)
{
	if ( fr == NULL ) return 0;
	if ( x < fr->x ) return 0;
	if ( y < fr->y ) return 0;
	if ( x > fr->x + fr->w ) return 0;
	if ( y > fr->y + fr->h ) return 0;
	return 1;
}


static struct frame *find_frame_at_position(struct frame *fr,
                                            double x, double y)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {

		if ( within_frame(fr->children[i], x, y) ) {
			return find_frame_at_position(fr->children[i], x, y);
		}

	}

	if ( within_frame(fr, x, y) ) return fr;
	return NULL;
}


static enum corner which_corner(double xp, double yp, struct frame *fr)
{
	double x, y;  /* Relative to object position */

	x = xp - fr->x;
	y = yp - fr->y;

	if ( x < 0.0 ) return CORNER_NONE;
	if ( y < 0.0 ) return CORNER_NONE;
	if ( x > fr->w ) return CORNER_NONE;
	if ( y > fr->h ) return CORNER_NONE;

	/* Top left? */
	if ( (x<20.0) && (y<20.0) ) return CORNER_TL;
	if ( (x>fr->w-20.0) && (y<20.0) ) return CORNER_TR;
	if ( (x<20.0) && (y>fr->h-20.0) ) return CORNER_BL;
	if ( (x>fr->w-20.0) && (y>fr->h-20.0) ) return CORNER_BR;

	return CORNER_NONE;
}


static void calculate_box_size(struct frame *fr, SCEditor *e,
                               double x, double y)
{
	double ddx, ddy, dlen, mult;
	double vx, vy, dbx, dby;

	ddx = x - e->start_corner_x;
	ddy = y - e->start_corner_y;

	if ( !fr->is_image ) {

		switch ( e->drag_corner ) {

			case CORNER_BR :
			e->box_x = fr->x;
			e->box_y = fr->y;
			e->box_width = fr->w + ddx;
			e->box_height = fr->h + ddy;
			break;

			case CORNER_BL :
			e->box_x = fr->x + ddx;
			e->box_y = fr->y;
			e->box_width = fr->w - ddx;
			e->box_height = fr->h + ddy;
			break;

			case CORNER_TL :
			e->box_x = fr->x + ddx;
			e->box_y = fr->y + ddy;
			e->box_width = fr->w - ddx;
			e->box_height = fr->h - ddy;
			break;

			case CORNER_TR :
			e->box_x = fr->x;
			e->box_y = fr->y + ddy;
			e->box_width = fr->w + ddx;
			e->box_height = fr->h - ddy;
			break;

			case CORNER_NONE :
			break;

		}
		return;


	}

	switch ( e->drag_corner ) {

		case CORNER_BR :
		vx = fr->w;
		vy = fr->h;
		break;

		case CORNER_BL :
		vx = -fr->w;
		vy = fr->h;
		break;

		case CORNER_TL :
		vx = -fr->w;
		vy = -fr->h;
		break;

		case CORNER_TR :
		vx = fr->w;
		vy = -fr->h;
		break;

		case CORNER_NONE :
		default:
		vx = 0.0;
		vy = 0.0;
		break;

	}

	dlen = (ddx*vx + ddy*vy) / e->diagonal_length;
	mult = (dlen+e->diagonal_length) / e->diagonal_length;

	e->box_width = fr->w * mult;
	e->box_height = fr->h * mult;
	dbx = e->box_width - fr->w;
	dby = e->box_height - fr->h;

	if ( e->box_width < 40.0 ) {
		mult = 40.0 / fr->w;
	}
	if ( e->box_height < 40.0 ) {
		mult = 40.0 / fr->h;
	}
	e->box_width = fr->w * mult;
	e->box_height = fr->h * mult;
	dbx = e->box_width - fr->w;
	dby = e->box_height - fr->h;

	switch ( e->drag_corner ) {

		case CORNER_BR :
		e->box_x = fr->x;
		e->box_y = fr->y;
		break;

		case CORNER_BL :
		e->box_x = fr->x - dbx;
		e->box_y = fr->y;
		break;

		case CORNER_TL :
		e->box_x = fr->x - dbx;
		e->box_y = fr->y - dby;
		break;

		case CORNER_TR :
		e->box_x = fr->x;
		e->box_y = fr->y - dby;
		break;

		case CORNER_NONE :
		break;

	}
}


static void check_paragraph(struct frame *fr, PangoContext *pc,
                            SCBlock *scblocks)
{
	if ( fr->n_paras > 0 ) return;
	Paragraph *para = last_open_para(fr);

	if ( scblocks == NULL ) {
		/* We have no SCBlocks at all!  Better create one... */
		scblocks = sc_parse("");
		fr->scblocks = scblocks;
	}

	/* We are creating the first paragraph.  It uses the last SCBlock
	 * in the chain */
	while ( sc_block_next(scblocks) != NULL ) {
		scblocks = sc_block_next(scblocks);
	}
	scblocks = sc_block_append(scblocks, NULL, NULL, strdup(""), NULL);

	add_run(para, scblocks, scblocks, fr->fontdesc, fr->col);
	wrap_paragraph(para, pc, fr->w - fr->pad_l - fr->pad_r, 0, 0);
}


static void rewrap_paragraph_range(struct frame *fr, int a, int b,
                                   struct edit_pos sel_start,
                                   struct edit_pos sel_end,
                                   int sel_active)
{
	int i;
	int sel_s, sel_e;
	Paragraph *para;

	if ( a > b ) {
		int t = a;
		a = b; b = t;
	}

	if ( fr == NULL ) return;
	if ( fr->paras == NULL ) return;

	sort_positions(&sel_start, &sel_end);

	//printf("frame %p\n", fr);
	//printf("start: ");
	//show_edit_pos(sel_start);
	//printf("  end: ");
	//show_edit_pos(sel_end);

	para = fr->paras[sel_start.para];
	sel_s = pos_trail_to_offset(para, sel_start.pos, sel_start.trail);
	para = fr->paras[sel_end.para];
	sel_e = pos_trail_to_offset(para, sel_end.pos, sel_end.trail);

	for ( i=a; i<=b; i++ ) {
		size_t srt, end;
		if ( sel_active ) {
			if ( i == sel_start.para ) {
				srt = sel_s;
			} else {
				srt = 0;
			}
			if ( i == sel_end.para ) {
				end = sel_e;
			} else {
				end = G_MAXUINT;
			}
			if ( i > sel_start.para && i < sel_end.para ) {
				end = G_MAXUINT;
			}
		} else {
			srt = 0;
			end = 0;
		}
		wrap_paragraph(fr->paras[i], NULL,
		               fr->w - fr->pad_l - fr->pad_r, srt, end);
	}
}


static void unset_selection(SCEditor *e)
{
	int a, b;

	if ( !e->sel_active ) return;

	a = e->sel_start.para;
	b = e->sel_end.para;
	if ( a > b ) {
		a = e->sel_end.para;
		b = e->sel_start.para;
	}
	e->sel_active = 0;
	rewrap_paragraph_range(e->cursor_frame, a, b, e->sel_start, e->sel_end, 0);
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 SCEditor *e)
{
	enum corner c;
	gdouble x, y;
	struct frame *clicked;
	int shift;

	x = event->x - e->border_offs_x;
	y = event->y - e->border_offs_y + e->scroll_pos;
	x /= e->view_scale;
	y /= e->view_scale;
	shift = event->state & GDK_SHIFT_MASK;

	if ( within_frame(e->selection, x, y) ) {
		clicked = e->selection;
	} else {
		clicked = find_frame_at_position(e->top, x, y);
	}

	/* Clicked within the currently selected frame
	 *   -> resize, move or select text */
	if ( (e->selection != NULL) && (clicked == e->selection) ) {

		struct frame *fr;

		fr = e->selection;

		/* Within the resizing region? */
		c = which_corner(x, y, fr);
		if ( (c != CORNER_NONE) && fr->resizable && shift ) {

			e->drag_reason = DRAG_REASON_RESIZE;
			e->drag_corner = c;

			e->start_corner_x = x;
			e->start_corner_y = y;
			e->diagonal_length = pow(fr->w, 2.0);
			e->diagonal_length += pow(fr->h, 2.0);
			e->diagonal_length = sqrt(e->diagonal_length);

			calculate_box_size(fr, e, x, y);

			e->drag_status = DRAG_STATUS_COULD_DRAG;
			e->drag_reason = DRAG_REASON_RESIZE;

		} else {

			/* Position cursor and prepare for possible drag */
			e->cursor_frame = clicked;
			check_paragraph(e->cursor_frame, e->pc, sc_block_child(fr->scblocks));
			find_cursor(clicked, x-fr->x, y-fr->y,
			            &e->cursor_para, &e->cursor_pos, &e->cursor_trail);

			e->start_corner_x = x;
			e->start_corner_y = y;

			if ( event->type == GDK_2BUTTON_PRESS ) {
				check_callback_click(e->cursor_frame, e->cursor_para);
			}

			if ( fr->resizable && shift ) {
				e->drag_status = DRAG_STATUS_COULD_DRAG;
				e->drag_reason = DRAG_REASON_MOVE;
			} else if ( !e->para_highlight ) {
				e->drag_status = DRAG_STATUS_COULD_DRAG;
				e->drag_reason = DRAG_REASON_TEXTSEL;
				unset_selection(e);
				find_cursor_2(clicked, x-fr->x, y-fr->y, &e->sel_start);
			}

		}

	} else if ( (clicked == NULL)
	         || ( !e->top_editable && (clicked == e->top) ) )
	{
		/* Clicked no object. Deselect old object.
		 * If shift held, set up for creating a new one. */
		e->selection = NULL;
		unset_selection(e);

		if ( shift ) {
			e->start_corner_x = x;
			e->start_corner_y = y;
			e->drag_status = DRAG_STATUS_COULD_DRAG;
			e->drag_reason = DRAG_REASON_CREATE;
		} else {
			e->drag_status = DRAG_STATUS_NONE;
			e->drag_reason = DRAG_REASON_NONE;
		}

	} else {

		/* Clicked an existing frame, no immediate dragging */
		e->drag_status = DRAG_STATUS_COULD_DRAG;
		e->drag_reason = DRAG_REASON_TEXTSEL;
		unset_selection(e);
		find_cursor_2(clicked, x-clicked->x, y-clicked->y,
		              &e->sel_start);
		find_cursor_2(clicked, x-clicked->x, y-clicked->y,
		              &e->sel_end);
		e->selection = clicked;
		e->cursor_frame = clicked;
		if ( clicked == e->top ) {
			check_paragraph(e->cursor_frame, e->pc, clicked->scblocks);
		} else {
			check_paragraph(e->cursor_frame, e->pc,
			                sc_block_child(clicked->scblocks));
		}
		find_cursor(clicked, x-clicked->x, y-clicked->y,
		            &e->cursor_para, &e->cursor_pos, &e->cursor_trail);

	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	sc_editor_redraw(e);
	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                           SCEditor *e)
{
	struct frame *fr = e->selection;
	gdouble x, y;

	x = event->x - e->border_offs_x;
	y = event->y - e->border_offs_y + e->scroll_pos;
	x /= e->view_scale;
	y /= e->view_scale;

	if ( e->drag_status == DRAG_STATUS_COULD_DRAG ) {

		/* We just got a motion signal, and the status was "could drag",
		 * therefore the drag has started. */
		e->drag_status = DRAG_STATUS_DRAGGING;

	}

	switch ( e->drag_reason ) {

		case DRAG_REASON_NONE :
		break;

		case DRAG_REASON_CREATE :
		e->drag_corner_x = x;
		e->drag_corner_y = y;
		sc_editor_redraw(e);
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled by dnd_motion() */
		break;

		case DRAG_REASON_RESIZE :
		calculate_box_size(fr, e,  x, y);
		sc_editor_redraw(e);
		break;

		case DRAG_REASON_MOVE :
		e->box_x = (fr->x - e->start_corner_x) + x;
		e->box_y = (fr->y - e->start_corner_y) + y;
		e->box_width = fr->w;
		e->box_height = fr->h;
		sc_editor_redraw(e);
		break;

		case DRAG_REASON_TEXTSEL :
		unset_selection(e);
		find_cursor_2(fr, x-fr->x, y-fr->y, &e->sel_end);
		rewrap_paragraph_range(fr, e->sel_start.para, e->sel_end.para,
		                       e->sel_start, e->sel_end, 1);
		find_cursor(fr, x-fr->x, y-fr->y, &e->cursor_para,
		            &e->cursor_pos, &e->cursor_trail);
		e->sel_active = !positions_equal(e->sel_start, e->sel_end);
		sc_editor_redraw(e);
		break;

	}

	gdk_event_request_motions(event);
	return FALSE;
}


static struct frame *create_frame(SCEditor *e, double x, double y,
                                  double w, double h)
{
	struct frame *parent;
	struct frame *fr;
	SCBlock *scblocks;

	parent = e->top;

	if ( w < 0.0 ) {
		x += w;
		w = -w;
	}

	if ( h < 0.0 ) {
		y += h;
		h = -h;
	}

	/* Add to frame structure */
	fr = add_subframe(parent);

	/* Add to SC */
	scblocks = sc_block_append_end(sc_block_child(e->scblocks), "f", NULL, NULL);
	fr->scblocks = scblocks;
	sc_block_append_inside(scblocks, NULL, NULL, strdup(""));

	fr->x = x;
	fr->y = y;
	fr->w = w;
	fr->h = h;
	fr->is_image = 0;
	fr->empty = 1;
	fr->resizable = 1;

	update_geom(fr);

	full_rerender(e);
	return find_frame_with_scblocks(e->top, scblocks);
}


static void do_resize(SCEditor *e, double x, double y, double w, double h)
{
	struct frame *fr;

	assert(e->selection != NULL);

	if ( w < 0.0 ) {
		w = -w;
		x -= w;
	}

	if ( h < 0.0 ) {
		h = -h;
		y -= h;
	}

	fr = e->selection;
	fr->x = x;
	fr->y = y;
	fr->w = w;
	fr->h = h;
	update_geom(fr);

	full_rerender(e);
	sc_editor_redraw(e);
}


static gboolean button_release_sig(GtkWidget *da, GdkEventButton *event,
                                   SCEditor *e)
{
	gdouble x, y;
	struct frame *fr;

	x = event->x - e->border_offs_x;
	y = event->y - e->border_offs_y;
	x /= e->view_scale;
	y /= e->view_scale;

	/* Not dragging?  Then I don't care. */
	if ( e->drag_status != DRAG_STATUS_DRAGGING ) return FALSE;

	e->drag_corner_x = x;
	e->drag_corner_y = y;
	e->drag_status = DRAG_STATUS_NONE;

	switch ( e->drag_reason )
	{

		case DRAG_REASON_NONE :
		printf("Release on pointless drag.\n");
		break;

		case DRAG_REASON_CREATE :
		fr = create_frame(e, e->start_corner_x, e->start_corner_y,
		                     e->drag_corner_x - e->start_corner_x,
		                     e->drag_corner_y - e->start_corner_y);
		if ( fr != NULL ) {
			check_paragraph(fr, e->pc, sc_block_child(fr->scblocks));
			e->selection = fr;
			e->cursor_frame = fr;
			e->cursor_para = 0;
			e->cursor_pos = 0;
			e->cursor_trail = 0;
		} else {
			fprintf(stderr, "Failed to create frame!\n");
		}
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled in dnd_drop() or dnd_leave() */
		break;

		case DRAG_REASON_RESIZE :
		do_resize(e, e->box_x, e->box_y, e->box_width, e->box_height);
		break;

		case DRAG_REASON_MOVE :
		do_resize(e, e->box_x, e->box_y, e->box_width, e->box_height);
		break;

		case DRAG_REASON_TEXTSEL :
		/* Do nothing (text is already selected) */
		break;

	}

	e->drag_reason = DRAG_REASON_NONE;

	gtk_widget_grab_focus(GTK_WIDGET(da));
	sc_editor_redraw(e);
	return FALSE;
}


static void copy_selection(SCEditor *e)
{
	GtkClipboard *cb;
	char *storycode;
	SCBlock *bl;

	bl = block_at_cursor(e->cursor_frame, e->cursor_para, 0);
	if ( bl == NULL ) return;

	storycode = serialise_sc_block(bl);

	cb = gtk_clipboard_get(GDK_NONE);
	gtk_clipboard_set_text(cb, storycode, -1);
	free(storycode);
}


static void paste_callback(GtkClipboard *cb, const gchar *text, void *vp)
{
	SCEditor *e = vp;
	SCBlock *bl = sc_parse(text);
	SCBlock *cur_bl;
	size_t cur_sc_pos;
	size_t offs;
	Paragraph *para;

	para = e->cursor_frame->paras[e->cursor_para];
	offs = pos_trail_to_offset(para, e->cursor_pos, e->cursor_trail);

	get_sc_pos(e->cursor_frame, e->cursor_para, offs, &cur_bl, &cur_sc_pos);
	sc_insert_block(cur_bl, cur_sc_pos, bl);
	full_rerender(e);
}


static void paste_selection(SCEditor *e)
{
	GtkClipboard *cb;

	cb = gtk_clipboard_get(GDK_NONE);
	gtk_clipboard_request_text(cb, paste_callback, e);
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              SCEditor *e)
{
	gboolean r;
	int claim = 0;

	/* Throw the event to the IM context and let it sort things out */
	r = gtk_im_context_filter_keypress(GTK_IM_CONTEXT(e->im_context),
		                           event);
	if ( r ) return FALSE;  /* IM ate it */

	switch ( event->keyval ) {

		case GDK_KEY_Escape :
		if ( !e->para_highlight ) {
			sc_editor_remove_cursor(e);
			sc_editor_redraw(e);
			claim = 1;
		}
		break;

		case GDK_KEY_Left :
		if ( e->selection != NULL ) {
			cursor_moveh(e->cursor_frame, &e->cursor_para,
			            &e->cursor_pos, &e->cursor_trail, -1);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Right :
		if ( e->selection != NULL ) {
			cursor_moveh(e->cursor_frame, &e->cursor_para,
			            &e->cursor_pos, &e->cursor_trail, +1);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Up :
		if ( e->selection != NULL ) {
			cursor_movev(e->cursor_frame, &e->cursor_para,
			            &e->cursor_pos, &e->cursor_trail, -1);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Down :
		if ( e->selection != NULL ) {
			cursor_movev(e->cursor_frame, &e->cursor_para,
			            &e->cursor_pos, &e->cursor_trail, +1);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;


		case GDK_KEY_Return :
		im_commit_sig(NULL, "\n", e);
		claim = 1;
		break;

		case GDK_KEY_BackSpace :
		if ( e->selection != NULL ) {
			do_backspace(e->selection, e);
			claim = 1;
		}
		break;

		case GDK_KEY_F5 :
		full_rerender(e);
		break;

		case GDK_KEY_F6 :
		show_cursor_pos(e);
		break;

		case GDK_KEY_F7 :
		if ( event->state & GDK_CONTROL_MASK ) {
			debug_paragraphs(e);
		} else if ( event->state & GDK_SHIFT_MASK ) {
			show_sc_blocks(e->scblocks);
		} else {
			open_debugger(e->cursor_frame);
		}
		break;

		case GDK_KEY_C :
		case GDK_KEY_c :
		if ( event->state == GDK_CONTROL_MASK ) {
			copy_selection(e);
		}
		break;

		case GDK_KEY_V :
		case GDK_KEY_v :
		if ( event->state == GDK_CONTROL_MASK ) {
			paste_selection(e);
		}
		break;


	}

	if ( claim ) return TRUE;
	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, SCEditor *e)
{
	GdkAtom target;

	/* If we haven't already requested the data, do so now */
	if ( !e->drag_preview_pending && !e->have_drag_data ) {

		target = gtk_drag_dest_find_target(widget, drag_context, NULL);

		if ( target != GDK_NONE ) {
			gtk_drag_get_data(widget, drag_context, target, time);
			e->drag_preview_pending = 1;
		} else {
			e->import_acceptable = 0;
			gdk_drag_status(drag_context, 0, time);
		}

	}

	if ( e->have_drag_data && e->import_acceptable ) {

		gdk_drag_status(drag_context, GDK_ACTION_LINK, time);
		e->start_corner_x = x - e->import_width/2.0;
		e->start_corner_y = y - e->import_height/2.0;
		e->drag_corner_x = x + e->import_width/2.0;
		e->drag_corner_y = y + e->import_height/2.0;

		sc_editor_redraw(e);

	}

	return TRUE;
}


static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                         gint x, gint y, guint time, SCEditor *e)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target(widget, drag_context, NULL);

	if ( target == GDK_NONE ) {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
	} else {
		gtk_drag_get_data(widget, drag_context, target, time);
	}

	return TRUE;
}


static void chomp(char *s)
{
	size_t i;

	if ( !s ) return;

	for ( i=0; i<strlen(s); i++ ) {
		if ( (s[i] == '\n') || (s[i] == '\r') ) {
			s[i] = '\0';
			return;
		}
	}
}


/* Scale the image down if it's a silly size */
static void check_import_size(SCEditor *e)
{
	if ( e->import_width > e->w ) {

		int new_import_width;

		new_import_width = e->w/2;
		e->import_height = (new_import_width * e->import_height) /
		                     e->import_width;
		e->import_width = new_import_width;

	}

	if ( e->import_height > e->h ) {

		int new_import_height;

		new_import_height = e->w/2;
		e->import_width = (new_import_height*e->import_width) /
		                    e->import_height;
		e->import_height = new_import_height;

	}
}


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, SCEditor *e)
{
	if ( e->drag_preview_pending ) {

		gchar *filename = NULL;
		GdkPixbufFormat *f;
		gchar **uris;
		int w, h;

		e->have_drag_data = 1;
		e->drag_preview_pending = 0;
		uris = gtk_selection_data_get_uris(seldata);
		if ( uris != NULL ) {
			filename = g_filename_from_uri(uris[0], NULL, NULL);
		}
		g_strfreev(uris);

		if ( filename == NULL ) {

			/* This doesn't even look like a sensible URI.
			 * Bail out. */
			gdk_drag_status(drag_context, 0, time);
			if ( e->drag_highlight ) {
				gtk_drag_unhighlight(widget);
				e->drag_highlight = 0;
			}
			e->import_acceptable = 0;
			return;

		}
		chomp(filename);

		f = gdk_pixbuf_get_file_info(filename, &w, &h);
		g_free(filename);

		e->import_width = w;
		e->import_height = h;

		if ( f == NULL ) {

			gdk_drag_status(drag_context, 0, time);
			if ( e->drag_highlight ) {
				gtk_drag_unhighlight(widget);
				e->drag_highlight = 0;
			}
			e->drag_status = DRAG_STATUS_NONE;
			e->drag_reason = DRAG_REASON_NONE;
			e->import_acceptable = 0;

		} else {

			/* Looks like a sensible image */
			gdk_drag_status(drag_context, GDK_ACTION_PRIVATE, time);
			e->import_acceptable = 1;

			if ( !e->drag_highlight ) {
				gtk_drag_highlight(widget);
				e->drag_highlight = 1;
			}

			check_import_size(e);
			e->drag_reason = DRAG_REASON_IMPORT;
			e->drag_status = DRAG_STATUS_DRAGGING;

		}

	} else {

		gchar **uris;
		char *filename = NULL;

		uris = gtk_selection_data_get_uris(seldata);
		if ( uris != NULL ) {
			filename = g_filename_from_uri(uris[0], NULL, NULL);
		}
		g_strfreev(uris);

		if ( filename != NULL ) {

			struct frame *fr;
			char *opts;
			size_t len;
			int w, h;

			gtk_drag_finish(drag_context, TRUE, FALSE, time);
			chomp(filename);

			w = e->drag_corner_x - e->start_corner_x;
			h = e->drag_corner_y - e->start_corner_y;

			len = strlen(filename)+64;
			opts = malloc(len);
			if ( opts == NULL ) {
				free(filename);
				fprintf(stderr, "Failed to allocate SC\n");
				return;
			}
			snprintf(opts, len, "1fx1f+0+0,filename=\"%s\"",
			         filename);

			fr = create_frame(e, e->start_corner_x,
			                     e->start_corner_y, w, h);
			fr->is_image = 1;
			fr->empty = 0;
			sc_block_append_inside(fr->scblocks, "image", opts, "");
			full_rerender(e); /* FIXME: No need for full */
			e->selection = NULL;
			e->cursor_frame = NULL;
			e->cursor_para = 0;
			e->cursor_pos = 0;
			sc_editor_redraw(e);
			free(filename);

		} else {

			gtk_drag_finish(drag_context, FALSE, FALSE, time);

		}

	}
}


static void dnd_leave(GtkWidget *widget, GdkDragContext *drag_context,
                      guint time, SCEditor *sceditor)
{
	if ( sceditor->drag_highlight ) {
		gtk_drag_unhighlight(widget);
	}
	sceditor->have_drag_data = 0;
	sceditor->drag_highlight = 0;
	sceditor->drag_status = DRAG_STATUS_NONE;
	sceditor->drag_reason = DRAG_REASON_NONE;
}


static gint realise_sig(GtkWidget *da, SCEditor *e)
{
	GdkWindow *win;

	/* Keyboard and input method stuff */
	e->im_context = gtk_im_multicontext_new();
	win = gtk_widget_get_window(GTK_WIDGET(e));
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(e->im_context), win);
	gdk_window_set_accept_focus(win, TRUE);
	g_signal_connect(G_OBJECT(e->im_context), "commit", G_CALLBACK(im_commit_sig), e);
	g_signal_connect(G_OBJECT(e), "key-press-event", G_CALLBACK(key_press_sig), e);

	/* FIXME: Can do this "properly" by setting up a separate font map */
	e->pc = gtk_widget_get_pango_context(GTK_WIDGET(e));

	return FALSE;
}


void sc_editor_set_scblock(SCEditor *e, SCBlock *scblocks)
{
	e->scblocks = scblocks;
	full_rerender(e);
}


SCBlock *sc_editor_get_scblock(SCEditor *e)
{
	return e->scblocks;
}


static void update_size_request(SCEditor *e)
{
	gtk_widget_set_size_request(GTK_WIDGET(e), 0, e->h + 2.0*e->min_border);
}


void sc_editor_set_size(SCEditor *e, int w, int h)
{
	e->w = w;
	e->h = h;
	update_size_request(e);
	if ( gtk_widget_get_mapped(GTK_WIDGET(e)) ) {
		full_rerender(e);
		sc_editor_redraw(e);
	}
}


void sc_editor_set_logical_size(SCEditor *e, double w, double h)
{
	e->log_w = w;
	e->log_h = h;
	if ( gtk_widget_get_mapped(GTK_WIDGET(e)) ) {
		full_rerender(e);
		sc_editor_redraw(e);
	}
}


void sc_editor_set_slidenum(SCEditor *e, int slidenum)
{
	e->slidenum = slidenum;
}


void sc_editor_set_min_border(SCEditor *e, double min_border)
{
	e->min_border = min_border;
	update_size_request(e);
}


void sc_editor_set_top_frame_editable(SCEditor *e, int top_frame_editable)
{
	e->top_editable = top_frame_editable;
}


static SCBlock **copy_ss_list(SCBlock **stylesheets)
{
	int n_ss = 0;
	int i = 0;
	SCBlock **ssc;

	if ( stylesheets == NULL ) return NULL;

	while ( stylesheets[n_ss] != NULL ) n_ss++;
	n_ss++;  /* One more for sentinel */

	ssc = malloc(n_ss*sizeof(SCBlock *));
	if ( ssc == NULL ) return NULL;

	for ( i=0; i<n_ss; i++ ) ssc[i] = stylesheets[i];

	return ssc;
}


void sc_editor_set_stylesheets(SCEditor *e, SCBlock **stylesheets)
{
	int i = 0;;
	while ( e->stylesheets[i] != NULL ) {
		sc_block_free(e->stylesheets[i++]);
	}
	free(e->stylesheets);
	e->stylesheets = copy_ss_list(stylesheets);
}


void sc_editor_set_callbacks(SCEditor *e, SCCallbackList *cbl)
{
	if ( e->cbl != NULL ) sc_callback_list_free(e->cbl);
	e->cbl = cbl;
}


void sc_editor_set_para_highlight(SCEditor *e, int para_highlight)
{
	e->para_highlight = para_highlight;
	sc_editor_redraw(e);
}

int sc_editor_get_cursor_para(SCEditor *e)
{
	if ( e->cursor_frame == NULL ) return 0;
	return e->cursor_para;
}


void *sc_editor_get_cursor_bvp(SCEditor *e)
{
	Paragraph *para;
	if ( e->cursor_frame == NULL ) return 0;
	para = e->cursor_frame->paras[e->cursor_para];
	return get_para_bvp(para);
}


void sc_editor_set_cursor_para(SCEditor *e, signed int pos)
{
	double h;
	int i;

	if ( e->cursor_frame == NULL ) {
		e->cursor_frame = e->top;
		e->selection = e->top;
	}

	if ( pos < 0 ) {
		e->cursor_para = e->cursor_frame->n_paras - 1;
	} else if ( pos >= e->cursor_frame->n_paras ) {
		e->cursor_para = e->cursor_frame->n_paras - 1;
	} else {
		e->cursor_para = pos;
	}
	e->cursor_pos = 0;
	e->cursor_trail = 0;

	h = 0;
	for ( i=0; i<e->cursor_para; i++ ) {
		h += paragraph_height(e->cursor_frame->paras[i]);
	}
	h += (paragraph_height(e->cursor_frame->paras[e->cursor_para]))/2;
	e->scroll_pos = h - (e->visible_height/2);
	set_vertical_params(e);

	sc_editor_redraw(e);
}


int sc_editor_get_num_paras(SCEditor *e)
{
	if ( e->cursor_frame == NULL ) return 1;
	return e->cursor_frame->n_paras;
}


void sc_editor_set_scale(SCEditor *e, int scale)
{
	e->scale = scale;
	if ( !scale ) {
		e->view_scale = 1.0;
	}
}


SCEditor *sc_editor_new(SCBlock *scblocks, SCBlock **stylesheets,
                        PangoLanguage *lang, const char *storename)
{
	SCEditor *sceditor;
	GtkTargetEntry targets[1];
	GError *err;

	sceditor = g_object_new(SC_TYPE_EDITOR, NULL);

	sceditor->scblocks = scblocks;
	sceditor->w = 100;
	sceditor->h = 100;
	sceditor->log_w = 100;
	sceditor->log_h = 100;
	sceditor->border_offs_x = 0;
	sceditor->border_offs_y = 0;
	sceditor->is = imagestore_new(storename);
	sceditor->slidenum = 0;
	sceditor->min_border = 0.0;
	sceditor->top_editable = 0;
	sceditor->cbl = NULL;
	sceditor->scroll_pos = 0;
	sceditor->flow = 0;
	sceditor->scale = 0;
	sceditor->view_scale = 1.0;
	sceditor->lang = lang;

	sceditor->para_highlight = 0;
	sceditor->cursor_frame = NULL;
	sceditor->cursor_para = 0;
	sceditor->cursor_pos = 0;
	sceditor->cursor_trail = 0;
	sceditor->selection = NULL;

	sceditor->stylesheets = copy_ss_list(stylesheets);

	sceditor->bg_pixbuf = NULL;

	err = NULL;
	sceditor->bg_pixbuf = gdk_pixbuf_new_from_file(DATADIR"/colloquium/sky.png", &err);
	if ( sceditor->bg_pixbuf == NULL ) {
		fprintf(stderr, "Failed to load background: %s\n",
		        err->message);
	}

	gtk_widget_set_size_request(GTK_WIDGET(sceditor),
	                            sceditor->w, sceditor->h);

	g_signal_connect(G_OBJECT(sceditor), "destroy",
	                 G_CALLBACK(destroy_sig), sceditor);
	g_signal_connect(G_OBJECT(sceditor), "realize",
	                 G_CALLBACK(realise_sig), sceditor);
	g_signal_connect(G_OBJECT(sceditor), "button-press-event",
	                 G_CALLBACK(button_press_sig), sceditor);
	g_signal_connect(G_OBJECT(sceditor), "button-release-event",
	                 G_CALLBACK(button_release_sig), sceditor);
	g_signal_connect(G_OBJECT(sceditor), "motion-notify-event",
	                 G_CALLBACK(motion_sig), sceditor);
	g_signal_connect(G_OBJECT(sceditor), "configure-event",
	                 G_CALLBACK(resize_sig), sceditor);

	/* Drag and drop */
	targets[0].target = "text/uri-list";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(GTK_WIDGET(sceditor), 0, targets, 1,
	                  GDK_ACTION_PRIVATE);
	g_signal_connect(sceditor, "drag-data-received",
	                 G_CALLBACK(dnd_receive), sceditor);
	g_signal_connect(sceditor, "drag-motion",
	                 G_CALLBACK(dnd_motion), sceditor);
	g_signal_connect(sceditor, "drag-drop",
	                 G_CALLBACK(dnd_drop), sceditor);
	g_signal_connect(sceditor, "drag-leave",
	                 G_CALLBACK(dnd_leave), sceditor);

	gtk_widget_set_can_focus(GTK_WIDGET(sceditor), TRUE);
	gtk_widget_add_events(GTK_WIDGET(sceditor),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
	                       | GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(sceditor), "draw",
			 G_CALLBACK(draw_sig), sceditor);

	gtk_widget_grab_focus(GTK_WIDGET(sceditor));

	gtk_widget_show(GTK_WIDGET(sceditor));

	return sceditor;
}
