/*
 * gtknarrativeview.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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

#include <presentation.h>

//#include "slide_window.h"
#include "gtknarrativeview.h"
//#include "slideshow.h"


static void scroll_interface_init(GtkScrollable *iface)
{
}


enum
{
	GTKNARRATIVEVIEW_0,
	GTKNARRATIVEVIEW_VADJ,
	GTKNARRATIVEVIEW_HADJ,
	GTKNARRATIVEVIEW_VPOL,
	GTKNARRATIVEVIEW_HPOL,
};


G_DEFINE_TYPE_WITH_CODE(GtkNarrativeView, gtk_narrative_view,
                        GTK_TYPE_DRAWING_AREA,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE,
                                              scroll_interface_init))

static void redraw(GtkNarrativeView *e)
{
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));
	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


static void horizontal_adjust(GtkAdjustment *adj, GtkNarrativeView *e)
{
	e->h_scroll_pos = gtk_adjustment_get_value(adj);
	redraw(e);
}


static void set_horizontal_params(GtkNarrativeView *e)
{
	if ( e->hadj == NULL ) return;
	gtk_adjustment_configure(e->hadj, e->h_scroll_pos, 0, e->w, 100,
	                         e->visible_width, e->visible_width);
}


static void vertical_adjust(GtkAdjustment *adj, GtkNarrativeView *e)
{
	e->scroll_pos = gtk_adjustment_get_value(adj);
	redraw(e);
}


static void set_vertical_params(GtkNarrativeView *e)
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


static void update_size(GtkNarrativeView *e)
{
//		double total = total_height(e->top);
//
//		e->w = e->top->w;
//		e->h = total + e->top->pad_t + e->top->pad_b;
//
//		e->top->h = e->h;
//
//	if ( e->top->h < e->visible_height ) {
//		e->top->h = e->visible_height;
//	}
//
//	set_vertical_params(e);
//	set_horizontal_params(e);
}


static gboolean resize_sig(GtkWidget *widget, GdkEventConfigure *event,
                           GtkNarrativeView *e)
{
	PangoContext *pc;

	pc = gdk_pango_context_get();

	e->visible_height = event->height;
	e->visible_width = event->width;

	/* Wrap everything with the current width, to get the total height */
	//e->top = interp_and_shape(e->scblocks, e->stylesheet, e->cbl,
	//                          e->is, e->slidenum, pc,
	//                          w, h, e->lang);
//		e->top->scblocks = e->scblocks;
//		recursive_wrap(e->top, pc);
//	}
//
//		/* Wrap using current width */
//		e->top->w = event->width;
//		e->top->h = 0.0;  /* To be updated in a moment */
//		e->top->x = 0.0;
//		e->top->y = 0.0;
//		/* Only the top level needs to be wrapped */
//		wrap_frame(e->top, pc);

	update_size(e);

	g_object_unref(pc);

	return FALSE;
}


static void emit_change_sig(GtkNarrativeView *e)
{
	g_signal_emit_by_name(e, "changed");
}


static void sc_editor_set_property(GObject *obj, guint id, const GValue *val,
                                   GParamSpec *spec)
{
	GtkNarrativeView *e = GTK_NARRATIVE_VIEW(obj);

	switch ( id ) {

		case GTKNARRATIVEVIEW_VPOL :
		e->vpol = g_value_get_enum(val);
		break;

		case GTKNARRATIVEVIEW_HPOL :
		e->hpol = g_value_get_enum(val);
		break;

		case GTKNARRATIVEVIEW_VADJ :
		e->vadj = g_value_get_object(val);
		set_vertical_params(e);
		if ( e->vadj != NULL ) {
			g_signal_connect(G_OBJECT(e->vadj), "value-changed",
			                 G_CALLBACK(vertical_adjust), e);
		}
		break;

		case GTKNARRATIVEVIEW_HADJ :
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
	GtkNarrativeView *e = GTK_NARRATIVE_VIEW(obj);

	switch ( id ) {

		case GTKNARRATIVEVIEW_VADJ :
		g_value_set_object(val, e->vadj);
		break;

		case GTKNARRATIVEVIEW_HADJ :
		g_value_set_object(val, e->hadj);
		break;

		case GTKNARRATIVEVIEW_VPOL :
		g_value_set_enum(val, e->vpol);
		break;

		case GTKNARRATIVEVIEW_HPOL :
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
	*min = 100;
	*natural = 640;
}


static void get_preferred_height(GtkWidget *widget, gint *min, gint *natural)
{
	*min = 1000;
	*natural = 1000;
}


static void gtk_narrative_view_class_init(GtkNarrativeViewClass *klass)
{
	GObjectClass *goc = G_OBJECT_CLASS(klass);
	goc->set_property = sc_editor_set_property;
	goc->get_property = sc_editor_get_property;
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_VADJ, "vadjustment");
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_HADJ, "hadjustment");
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_VPOL, "vscroll-policy");
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_HPOL, "hscroll-policy");

	GTK_WIDGET_CLASS(klass)->get_request_mode = get_request_mode;
	GTK_WIDGET_CLASS(klass)->get_preferred_width = get_preferred_width;
	GTK_WIDGET_CLASS(klass)->get_preferred_height = get_preferred_height;
	GTK_WIDGET_CLASS(klass)->get_preferred_height_for_width = NULL;

	g_signal_new("changed", GTK_TYPE_NARRATIVE_VIEW, G_SIGNAL_RUN_LAST, 0,
	             NULL, NULL, NULL, G_TYPE_NONE, 0);
}


static void gtk_narrative_view_init(GtkNarrativeView *e)
{
	e->vpol = GTK_SCROLL_NATURAL;
	e->hpol = GTK_SCROLL_NATURAL;
	e->vadj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
	e->hadj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
}


static void paste_storycode_received(GtkClipboard *cb, GtkSelectionData *seldata,
                                     gpointer vp)
{
//	GtkNarrativeView *e = vp;
//	SCBlock *nf;
//	const guchar *t;
//
//	t = gtk_selection_data_get_data(seldata);
//
//	printf("received storycode paste\n");
//	printf("'%s'\n", t);
//	if ( t == NULL ) return;
//
//	/* FIXME: It might not be a new frame */
//	nf = sc_parse((char *)t);
//	show_sc_blocks(nf);
//	sc_block_append_block(sc_block_child(e->scblocks), nf);
//	full_rerender(e);
}


static void paste_text_received(GtkClipboard *cb, GtkSelectionData *seldata,
                                gpointer vp)
{
//	GtkNarrativeView *e = vp;
//	SCBlock *bl;
//	guchar *t;
//	SCBlock *cur_bl;
//	size_t cur_sc_pos;
//	size_t offs;
//	Paragraph *para;
//
//	t = gtk_selection_data_get_text(seldata);
//
//	printf("received text paste\n");
//	printf("'%s'\n", t);
//	if ( t == NULL ) return;
//
//	bl = sc_parse((char *)t);
//
//	if ( e->cursor_frame == NULL ) {
//		fprintf(stderr, _("No frame selected for paste\n"));
//		return;
//	}
//
//	para = e->cursor_frame->paras[e->cpos.para];
//	offs = pos_trail_to_offset(para, e->cpos.pos, e->cpos.trail);
//
//	get_sc_pos(e->cursor_frame, e->cpos.para, offs, &cur_bl, &cur_sc_pos);
//	sc_insert_block(cur_bl, cur_sc_pos, bl);
//	full_rerender(e);
}


static void paste_targets_received(GtkClipboard *cb, GdkAtom *targets,
                                   gint n_targets, gpointer vp)
{
	GtkNarrativeView *e = vp;
	int i;
	int have_sc = 0;
	int index_sc, index_text;
	int have_text = 0;

	if ( targets == NULL ) {
		fprintf(stderr, "No paste targets offered.\n");
		return;
	}

	for ( i=0; i<n_targets; i++ ) {
		gchar *name = gdk_atom_name(targets[i]);
		if ( g_strcmp0(name, "text/x-storycode") == 0 ) {
			have_sc = 1;
			index_sc = i;
		}
		if ( g_strcmp0(name, "text/plain") == 0 ) {
			have_text = 1;
			index_text = i;
		}
		g_free(name);
	}

	if ( have_sc ) {
		printf("storycode is offered\n");
		gtk_clipboard_request_contents(cb, targets[index_sc],
		                               paste_storycode_received, e);
	} else if ( have_text ) {
		printf("text is offered\n");
		gtk_clipboard_request_contents(cb, targets[index_text],
		                               paste_text_received, e);
	} else {
		printf("nothing useful is offered\n");
	}
}


void sc_editor_paste(GtkNarrativeView *e)
{
	GtkClipboard *cb;
	GdkAtom atom;

	printf("pasting\n");

	atom = gdk_atom_intern("CLIPBOARD", FALSE);
	if ( atom == GDK_NONE ) return;
	cb = gtk_clipboard_get(atom);
	gtk_clipboard_request_targets(cb, paste_targets_received, e);
}


static void clipboard_get(GtkClipboard *cb, GtkSelectionData *seldata,
                          guint info, gpointer data)
{
	char *t = data;

	printf("clipboard get\n");

	if ( info == 0 ) {
		printf("sending SC frame\n");
		gtk_selection_data_set(seldata,
		                       gtk_selection_data_get_target(seldata),
		                       8, (const guchar *)t, strlen(t)+1);
	} else {
		GdkAtom target;
		gchar *name;
		target = gtk_selection_data_get_target(seldata);
		name = gdk_atom_name(target);
		fprintf(stderr, "Don't know what to send for %s\n", name);
		g_free(name);
	}
}


static void clipboard_clear(GtkClipboard *cb, gpointer data)
{
	free(data);
}


static void copy_selection(GtkNarrativeView *e)
{
//	char *t;
//	GtkClipboard *cb;
//	GdkAtom atom;
//	GtkTargetEntry targets[1];
//
//	atom = gdk_atom_intern("CLIPBOARD", FALSE);
//	if ( atom == GDK_NONE ) return;
//
//	cb = gtk_clipboard_get(atom);
//
//	targets[0].target = "text/x-storycode";
//	targets[0].flags = 0;
//	targets[0].info = 0;
//
//	printf("copying selection\n");
//
//	bl = block_at_cursor(e->cursor_frame, e->cpos.para, 0);
//	if ( bl == NULL ) return;
//
//	t = serialise_sc_block(bl);
//
//	gtk_clipboard_set_with_data(cb, targets, 1,
//	                            clipboard_get, clipboard_clear, t);
}


static gint destroy_sig(GtkWidget *window, GtkNarrativeView *e)
{
	return 0;
}


//static void draw_para_highlight(cairo_t *cr, struct frame *fr, int cursor_para)
//{
//	double cx, cy, w, h;
//
//	if ( get_para_highlight(fr, cursor_para, &cx, &cy, &w, &h) != 0 ) {
//		return;
//	}
//
//	cairo_new_path(cr);
//	cairo_rectangle(cr, cx+fr->x, cy+fr->y, w, h);
//	cairo_set_source_rgba(cr, 0.7, 0.7, 1.0, 0.5);
//	cairo_set_line_width(cr, 5.0);
//	cairo_stroke(cr);
//}


//static void draw_caret(cairo_t *cr, struct frame *fr, struct edit_pos cpos,
//                       int hgh)
//{
//	double cx, clow, chigh, h;
//	const double t = 1.8;
//	size_t offs;
//	Paragraph *para;
//
//	if ( hgh ) {
//		draw_para_highlight(cr, fr, cpos.para);
//		return;
//	}
//
//	assert(fr != NULL);
//
//	para = fr->paras[cpos.para];
//	if ( para_type(para) != PARA_TYPE_TEXT ) {
//		draw_para_highlight(cr, fr, cpos.para);
//		return;
//	}
//
//	offs = pos_trail_to_offset(para, cpos.pos, cpos.trail);
//	get_cursor_pos(fr, cpos.para, offs, &cx, &clow, &h);
//
//	cx += fr->x;
//	clow += fr->y;
//	chigh = clow + h;
//
//	cairo_move_to(cr, cx, clow);
//	cairo_line_to(cr, cx, chigh);
//
//	cairo_move_to(cr, cx-t, clow-t);
//	cairo_line_to(cr, cx, clow);
//	cairo_move_to(cr, cx+t, clow-t);
//	cairo_line_to(cr, cx, clow);
//
//	cairo_move_to(cr, cx-t, chigh+t);
//	cairo_line_to(cr, cx, chigh);
//	cairo_move_to(cr, cx+t, chigh+t);
//	cairo_line_to(cr, cx, chigh);
//
//	cairo_set_source_rgb(cr, 0.86, 0.0, 0.0);
//	cairo_set_line_width(cr, 1.0);
//	cairo_stroke(cr);
//}


static void draw_overlay(cairo_t *cr, GtkNarrativeView *e)
{
	//draw_caret(cr, e->cursor_frame, e->cpos, e->para_highlight);
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, GtkNarrativeView *e)
{
	/* Ultimate background */
	cairo_set_source_rgba(cr, 0.8, 0.8, 1.0, 1.0);
	cairo_paint(cr);

	cairo_translate(cr, -e->h_scroll_pos, -e->scroll_pos);

	/* Rendering background */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, e->w, e->h);
	cairo_fill(cr);

	/* Contents */

	/* Editing overlay */
	draw_overlay(cr, e);

	return FALSE;
}


static void check_cursor_visible(GtkNarrativeView *e)
{
//	double x, y, h;
//	size_t offs;
//	Paragraph *para;
//
//	if ( e->cursor_frame == NULL ) return;
//
//	para = e->cursor_frame->paras[e->cpos.para];
//	offs = pos_trail_to_offset(para, e->cpos.pos, e->cpos.trail);
//	get_cursor_pos(e->cursor_frame, e->cpos.para, offs, &x, &y, &h);
//
//	/* Off the bottom? */
//	if ( y - e->scroll_pos + h > e->visible_height ) {
//		e->scroll_pos = y + h - e->visible_height;
//		e->scroll_pos += e->cursor_frame->pad_b;
//	}
//
//	/* Off the top? */
//	if ( y < e->scroll_pos ) {
//		e->scroll_pos = y - e->cursor_frame->pad_t;
//	}
}


static void do_backspace(GtkNarrativeView *e)
{
//	double wrapw = e->cursor_frame->w - e->cursor_frame->pad_l - e->cursor_frame->pad_r;
//
//	if ( e->sel_active ) {
//
//		/* Delete the selected block */
//		delete_text_from_frame(e->cursor_frame, e->sel_start, e->sel_end, wrapw);
//
//		/* Cursor goes at start of deletion */
//		sort_positions(&e->sel_start, &e->sel_end);
//		e->cpos = e->sel_start;
//		e->sel_active = 0;
//
//	} else {
//
//		if ( para_type(e->cursor_frame->paras[e->cpos.para]) == PARA_TYPE_TEXT ) {
//
//			/* Delete one character */
//			struct edit_pos p1, p2;
//
//			p1 = e->cpos;
//
//			p2 = p1;
//
//			cursor_moveh(e->cursor_frame, &p2, -1);
//			show_edit_pos(p1);
//			show_edit_pos(p2);
//
//			delete_text_from_frame(e->cursor_frame, p1, p2, wrapw);
//			e->cpos = p2;
//
//		} else {
//
//			/* FIXME: Implement this */
//			fprintf(stderr, "Deleting non-text paragraph\n");
//
//		}
//
//	}

	emit_change_sig(e);
	redraw(e);
}


static void insert_text(char *t, GtkNarrativeView *e)
{
//	Paragraph *para;
//
//	if ( e->cursor_frame == NULL ) return;
//
//	if ( e->sel_active ) {
//		do_backspace(e->cursor_frame, e);
//	}
//
//	if ( strcmp(t, "\n") == 0 ) {
//		split_paragraph_at_cursor(e);
//		update_size(e);
//		cursor_moveh(e->cursor_frame, &e->cpos, +1);
//		check_cursor_visible(e);
//		emit_change_sig(e);
//		redraw(e);
//		return;
//	}
//
//	para = e->cursor_frame->paras[e->cpos.para];
//
//	/* Is this paragraph even a text one? */
//	if ( para_type(para) == PARA_TYPE_TEXT ) {
//
//		size_t off;
//
//		/* Yes. The "easy" case */
//
//		if ( !position_editable(e->cursor_frame, e->cpos) ) {
//			fprintf(stderr, "Position not editable\n");
//			return;
//		}
//
//		off = pos_trail_to_offset(para, e->cpos.pos, e->cpos.trail);
//		insert_text_in_paragraph(para, off, t);
//		wrap_paragraph(para, NULL,
//		               e->cursor_frame->w - e->cursor_frame->pad_l
//		                            - e->cursor_frame->pad_r, 0, 0);
//		update_size(e);
//
//		cursor_moveh(e->cursor_frame, &e->cpos, +1);
//
//	} else {
//
//		SCBlock *bd;
//		SCBlock *ad;
//		Paragraph *pnew;
//
//		bd = para_scblock(para);
//		if ( bd == NULL ) {
//			fprintf(stderr, "No SCBlock for para\n");
//			return;
//		}
//
//		/* No. Create a new text paragraph straight afterwards */
//		ad = sc_block_insert_after(bd, NULL, NULL, strdup(t));
//		if ( ad == NULL ) {
//			fprintf(stderr, "Failed to add SCBlock\n");
//			return;
//		}
//
//		pnew = insert_paragraph(e->cursor_frame, e->cpos.para);
//		if ( pnew == NULL ) {
//			fprintf(stderr, "Failed to insert paragraph\n");
//			return;
//		}
//		add_run(pnew, ad, e->cursor_frame->fontdesc,
//		        e->cursor_frame->col, NULL);
//
//		wrap_frame(e->cursor_frame, e->pc);
//
//		e->cpos.para += 1;
//		e->cpos.pos = 0;
//		e->cpos.trail = 1;
//
//	}
//
	emit_change_sig(e);
	check_cursor_visible(e);
	redraw(e);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              GtkNarrativeView *e)
{
	insert_text(str, e);
	return FALSE;
}


static void unset_selection(GtkNarrativeView *e)
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
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 GtkNarrativeView *e)
{
//	enum corner c;
//	gdouble x, y;
//	struct frame *clicked;
//	int shift;
//
//	x = event->x;
//	y = event->y + e->scroll_pos;
//	shift = event->state & GDK_SHIFT_MASK;
//
//	/* Clicked within the currently selected frame
//	 *   -> resize, move or select text */
//	if ( (e->selection != NULL) && (clicked == e->selection) ) {
//
//		struct frame *fr;
//
//		fr = e->selection;
//
//		/* Within the resizing region? */
//		c = which_corner(x, y, fr);
//		if ( (c != CORNER_NONE) && fr->resizable && shift ) {
//
//			e->drag_reason = DRAG_REASON_RESIZE;
//			e->drag_corner = c;
//
//			e->start_corner_x = x;
//			e->start_corner_y = y;
//			e->diagonal_length = pow(fr->w, 2.0);
//			e->diagonal_length += pow(fr->h, 2.0);
//			e->diagonal_length = sqrt(e->diagonal_length);
//
//			calculate_box_size(fr, e, x, y);
//
//			e->drag_status = DRAG_STATUS_COULD_DRAG;
//			e->drag_reason = DRAG_REASON_RESIZE;
//
//		} else {
//
//			/* Position cursor and prepare for possible drag */
//			e->cursor_frame = clicked;
//			check_paragraph(e->cursor_frame, e->pc, sc_block_child(fr->scblocks));
//			find_cursor(clicked, x-fr->x, y-fr->y, &e->cpos);
//			ensure_run(e->cursor_frame, e->cpos);
//
//			e->start_corner_x = x;
//			e->start_corner_y = y;
//
//			if ( event->type == GDK_2BUTTON_PRESS ) {
//				check_callback_click(e->cursor_frame, e->cpos.para);
//			}
//
//			if ( fr->resizable && shift ) {
//				e->drag_status = DRAG_STATUS_COULD_DRAG;
//				e->drag_reason = DRAG_REASON_MOVE;
//			} else if ( !e->para_highlight ) {
//				e->drag_status = DRAG_STATUS_COULD_DRAG;
//				e->drag_reason = DRAG_REASON_TEXTSEL;
//				unset_selection(e);
//				find_cursor(clicked, x-fr->x, y-fr->y, &e->sel_start);
//			}
//
//		}
//
//	} else if ( (clicked == NULL)
//	         || ( !e->top_editable && (clicked == e->top) ) )
//	{
//		/* Clicked no object. Deselect old object.
//		 * If shift held, set up for creating a new one. */
//		e->selection = NULL;
//		unset_selection(e);
//
//		if ( shift ) {
//			e->start_corner_x = x;
//			e->start_corner_y = y;
//			e->drag_status = DRAG_STATUS_COULD_DRAG;
//			e->drag_reason = DRAG_REASON_CREATE;
//		} else {
//			e->drag_status = DRAG_STATUS_NONE;
//			e->drag_reason = DRAG_REASON_NONE;
//		}
//
//	} else {
//
//		/* Clicked an existing frame, no immediate dragging */
//		e->drag_status = DRAG_STATUS_COULD_DRAG;
//		e->drag_reason = DRAG_REASON_TEXTSEL;
//		unset_selection(e);
//		find_cursor(clicked, x-clicked->x, y-clicked->y,
//		            &e->sel_start);
//		find_cursor(clicked, x-clicked->x, y-clicked->y,
//		            &e->sel_end);
//		e->selection = clicked;
//		e->cursor_frame = clicked;
//		if ( clicked == e->top ) {
//			check_paragraph(e->cursor_frame, e->pc, clicked->scblocks);
//		} else {
//			check_paragraph(e->cursor_frame, e->pc,
//			                sc_block_child(clicked->scblocks));
//		}
//		find_cursor(clicked, x-clicked->x, y-clicked->y, &e->cpos);
//		ensure_run(e->cursor_frame, e->cpos);
//
//	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw(e);
	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                           GtkNarrativeView *e)
{
	if ( e->drag_status == DRAG_STATUS_COULD_DRAG ) {

		/* We just got a motion signal, and the status was "could drag",
		 * therefore the drag has started. */
		e->drag_status = DRAG_STATUS_DRAGGING;

	}

	switch ( e->drag_reason ) {

		case DRAG_REASON_NONE :
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled by dnd_motion() */
		break;

		case DRAG_REASON_TEXTSEL :
		//unset_selection(e);
		//find_cursor(fr, x-fr->x, y-fr->y, &e->sel_end);
		//rewrap_paragraph_range(fr, e->sel_start.para, e->sel_end.para,
		//                       e->sel_start, e->sel_end, 1);
		//find_cursor(fr, x-fr->x, y-fr->y, &e->cpos);
		//e->sel_active = !positions_equal(e->sel_start, e->sel_end);
		redraw(e);
		break;

	}

	gdk_event_request_motions(event);
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              GtkNarrativeView *e)
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
			//sc_editor_remove_cursor(e);
			redraw(e);
			claim = 1;
		}
		break;

		case GDK_KEY_Left :
		//cursor_moveh(e->cursor_frame, &e->cpos, -1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Right :
		//cursor_moveh(e->cursor_frame, &e->cpos, +1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Up :
		//cursor_moveh(e->cursor_frame, &e->cpos, -1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Down :
		//cursor_moveh(e->cursor_frame, &e->cpos, +1);
		redraw(e);
		claim = 1;
		break;


		case GDK_KEY_Return :
		im_commit_sig(NULL, "\n", e);
		claim = 1;
		break;

		case GDK_KEY_BackSpace :
		//do_backspace(e->selection, e);
		claim = 1;
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
			sc_editor_paste(e);
		}
		break;


	}

	if ( claim ) return TRUE;
	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, GtkNarrativeView *e)
{
	return TRUE;
}


static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                         gint x, gint y, guint time, GtkNarrativeView *e)
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


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, GtkNarrativeView *e)
{
}


static void dnd_leave(GtkWidget *widget, GdkDragContext *drag_context,
                      guint time, GtkNarrativeView *nview)
{
	nview->drag_status = DRAG_STATUS_NONE;
	nview->drag_reason = DRAG_REASON_NONE;
}


static gint realise_sig(GtkWidget *da, GtkNarrativeView *e)
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


static void update_size_request(GtkNarrativeView *e)
{
	gtk_widget_set_size_request(GTK_WIDGET(e), 0, e->h);
}


GtkNarrativeView *gtk_narrative_view_new(Presentation *p, PangoLanguage *lang,
                                         const char *storename)
{
	GtkNarrativeView *nview;
	GtkTargetEntry targets[1];

	nview = g_object_new(GTK_TYPE_NARRATIVE_VIEW, NULL);

	nview->w = 100;
	nview->h = 100;
	nview->scroll_pos = 0;
	nview->lang = lang;

	nview->para_highlight = 0;

	gtk_widget_set_size_request(GTK_WIDGET(nview),
	                            nview->w, nview->h);

	g_signal_connect(G_OBJECT(nview), "destroy",
	                 G_CALLBACK(destroy_sig), nview);
	g_signal_connect(G_OBJECT(nview), "realize",
	                 G_CALLBACK(realise_sig), nview);
	g_signal_connect(G_OBJECT(nview), "button-press-event",
	                 G_CALLBACK(button_press_sig), nview);
	g_signal_connect(G_OBJECT(nview), "motion-notify-event",
	                 G_CALLBACK(motion_sig), nview);
	g_signal_connect(G_OBJECT(nview), "configure-event",
	                 G_CALLBACK(resize_sig), nview);

	/* Drag and drop */
	targets[0].target = "text/uri-list";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(GTK_WIDGET(nview), 0, targets, 1,
	                  GDK_ACTION_PRIVATE);
	g_signal_connect(nview, "drag-data-received",
	                 G_CALLBACK(dnd_receive), nview);
	g_signal_connect(nview, "drag-motion",
	                 G_CALLBACK(dnd_motion), nview);
	g_signal_connect(nview, "drag-drop",
	                 G_CALLBACK(dnd_drop), nview);
	g_signal_connect(nview, "drag-leave",
	                 G_CALLBACK(dnd_leave), nview);

	gtk_widget_set_can_focus(GTK_WIDGET(nview), TRUE);
	gtk_widget_add_events(GTK_WIDGET(nview),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
	                       | GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(nview), "draw",
			 G_CALLBACK(draw_sig), nview);

	gtk_widget_grab_focus(GTK_WIDGET(nview));

	gtk_widget_show(GTK_WIDGET(nview));

	return nview;
}
