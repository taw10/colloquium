/*
 * sc_editor.c
 *
 * Copyright © 2013-2016 Thomas White <taw@bitwiz.org.uk>
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
#include "wrap.h"
#include "sc_parse.h"
#include "sc_interp.h"
#include "sc_editor.h"
#include "slideshow.h"
#include "shape.h"
#include "boxvec.h"


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
	if ( e->vadj == NULL ) return;
	gtk_adjustment_configure(e->vadj, e->scroll_pos, 0, e->h, 100,
	                         e->visible_height, e->visible_height);
}


static void update_size(SCEditor *e)
{
	if ( e->flow ) {
		e->w = e->top->w;
		e->h = total_height(e->top);
		e->log_w = e->w;
		e->log_h = e->h;
		e->top->h = e->h;
	} else {
		e->top->w = e->log_w;
		e->top->h = e->log_h;
	}

	set_vertical_params(e);
	set_horizontal_params(e);
}


static gboolean resize_sig(GtkWidget *widget, GdkEventConfigure *event,
                           SCEditor *e)
{
	e->visible_height = event->height;
	e->visible_width = event->width;

	/* Interpret and shape, if not already done */
	if ( e->top == NULL ) {
		cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
		double w, h;
		if ( e->flow ) {
			w = event->width;
			h = 0.0;
		} else {
			w = e->log_w;
			h = e->log_h;
		}
		e->top = interp_and_shape(e->scblocks, e->stylesheets, e->cbl,
		                          e->is, ISZ_EDITOR, 0, cr, w, h,
		                          e->lang);
		recursive_wrap(e->top, e->is, ISZ_EDITOR);
		cairo_destroy(cr);
	}

	if ( e->flow ) {
		/* Wrap using current width */
		e->top->w = event->width;
		e->top->h = 0.0;  /* To be updated in a moment */
		e->top->x = 0.0;
		e->top->y = 0.0;
		wrap_contents(e->top); /* Only the top level needs to be wrapped */
	}

	update_size(e);

	return FALSE;
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
	e->cursor_line = 0;
	e->cursor_pos = 0;
	e->selection = NULL;
}


/* (Re-)run the entire rendering pipeline.
 * NB "full" means "full".  All frame, line and box handles will become
 * invalid.  The cursor position will be unset. */
static void full_rerender(SCEditor *e)
{
	frame_free(e->top);
	e->cursor_frame = NULL;
	e->cursor_line = 0;
	e->cursor_pos = 0;
	e->selection = NULL;

	cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(GTK_WIDGET(e)));
	e->top = interp_and_shape(e->scblocks, e->stylesheets, e->cbl,
	                          e->is, ISZ_EDITOR, 0, cr, e->w, 0.0, e->lang);
	cairo_destroy(cr);

	e->top->x = 0.0;
	e->top->y = 0.0;
	e->top->w = e->w;
	e->top->h = 0.0;  /* To be updated in a moment */

	recursive_wrap(e->top, e->is, ISZ_EDITOR);
	update_size(e);

	sc_editor_redraw(e);
}


void sc_editor_redraw(SCEditor *e)
{
	gint w, h;

	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));

	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


void sc_editor_delete_selected_frame(SCEditor *e)
{
	sc_block_delete(e->scblocks, e->selection->scblocks);
	full_rerender(e);
}


static void move_cursor_back(SCEditor *e)
{
	int retreat = 0;
	signed int cp, cb, cl;
	struct wrap_line *line;
	struct wrap_box *box;

	cp = e->cursor_pos;
	cb = e->cursor_box;
	cl = e->cursor_line;

	line = &e->cursor_frame->lines[e->cursor_line];
	box = bv_box(line->boxes, e->cursor_box);
	if ( box->type == WRAP_BOX_PANGO ) {

		if ( cp == 0 ) {
			retreat = 1;
		} else {
			cp--;
		}

	} else {
		cp--;
		if ( cp < 0 ) retreat = 1;
	}

	if ( retreat ) {

		do {

			cb--;

			if ( cb < 0 ) {
				cl--;
				if ( cl < 0 ) return;
				e->cursor_line = cl;
				line = &e->cursor_frame->lines[cl];
				cb = bv_len(line->boxes) - 1;
			}

		} while ( !bv_box(line->boxes, cb)->editable );

		e->cursor_box = cb;
		box = bv_box(line->boxes, cb);
		if ( box->type == WRAP_BOX_PANGO ) {
			cp = box->len_chars;
			if ( box->space == WRAP_SPACE_NONE ) {
				cp--;
			}
		} else {
			cp = 0;
		}

	}
	e->cursor_pos = cp;
}


void cur_box_diag(SCEditor *e)
{
	int sln, sbx, sps;
	struct frame *fr;

	fr = e->cursor_frame;
	sln = e->cursor_line;
	sbx = e->cursor_box;
	sps = e->cursor_pos;

	if ( fr->lines == NULL ) {
		printf("Current frame has no lines!\n");
		return;
	}

	if ( fr->lines[sln].boxes == NULL ) {
		printf("line %i of %i, but it has no boxes\n",
		       sln, fr->n_lines);
		return;
	}

	struct wrap_box *sbox = bv_box(fr->lines[sln].boxes, sbx);

	if ( sbox == NULL ) {
		printf("line/box: [%i of %i]/[%i of %i]/NULL box\n",
		       sln, fr->n_lines, sbx,
		       bv_len(e->cursor_frame->lines[sln].boxes));
	       return;
	}

	printf("line/box/pos: [%i of %i]/[%i of %i]/[%i of %i]\n",
	       sln, fr->n_lines, sbx, bv_len(e->cursor_frame->lines[sln].boxes),
	       sps, sbox->len_chars);
	printf("box type is %i, space type is %i\n", sbox->type, sbox->space);
	if ( sbox->type == WRAP_BOX_NOTHING ) {
		printf("Warning: in a nothing box!\n");
	}

	struct wrap_line *ln = &fr->lines[sln];
	int i;
	for ( i=0; i<bv_len(ln->boxes); i++ ) {
		char pp = '[';
		char pq = ']';
		struct wrap_box *bx = bv_box(ln->boxes, i);
		if ( i == sbx ) { pp = '<'; pq  = '>'; }
		printf("%c%i %i %i%c", pp, bx->offs_char, bx->len_chars,
		       bx->n_segs, pq);
	}
	printf("\n");
}


void advance_cursor(SCEditor *e)
{
	int advance = 0;
	signed int cp, cb, cl;
	struct wrap_line *line = &e->cursor_frame->lines[e->cursor_line];
	struct wrap_box *box = bv_box(line->boxes, e->cursor_box);

	cp = e->cursor_pos;
	cb = e->cursor_box;
	cl = e->cursor_line;

	/* FIXME: For Pango boxes, we should be counting cursor positions, not
	 * characters */

	switch ( box->type ) {

		case WRAP_BOX_PANGO:
		if ( cp+1 > box->len_chars ) {
			advance = 1;
		} else {
			cp++;
		}
		break;

		case WRAP_BOX_NOTHING:
		case WRAP_BOX_SENTINEL:
		advance = 1;
		break;

		case WRAP_BOX_IMAGE:
		case WRAP_BOX_CALLBACK:
		cp++;
		if ( cp > 1 ) advance = 1;
		break;

	}

	if ( advance ) {

		do {

			cb++;
			cp = 0;

			if ( box->space == WRAP_SPACE_NONE ) {
				cp = 1;
			}

			if ( cb >= bv_len(line->boxes) ) {
				cl++;
				if ( cl >= e->cursor_frame->n_lines ) {
					/* Give up - could not move */
					return;
				}
				line = &e->cursor_frame->lines[cl];
				cb = 0;
				cp = 0;
			}

		} while ( !bv_box(line->boxes, cb)->editable );

		e->cursor_line = cl;
		e->cursor_box = cb;

	}
	e->cursor_pos = cp;
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


static void draw_caret(cairo_t *cr, struct frame *fr,
                       int cursor_line, int cursor_box, int cursor_pos)
{
	double xposd, yposd, line_height;
	double cx, clow, chigh;
	const double t = 1.8;
	struct wrap_box *box;
	int i;

	if ( fr == NULL ) return;
	if ( fr->n_lines == 0 ) return;
	if ( fr->lines == NULL ) return;
	if ( fr->lines[cursor_line].boxes == NULL ) return;

	/* Locate the cursor in a "logical" and "geographical" sense */
	box = bv_box(fr->lines[cursor_line].boxes, cursor_box);
	get_cursor_pos(box, cursor_pos, &xposd, &yposd, &line_height);
	xposd += fr->pad_l;
	yposd += fr->pad_t;

	for ( i=0; i<cursor_line; i++ ) {
		yposd += pango_units_to_double(fr->lines[i].height);
	}

	for ( i=0; i<cursor_box; i++ ) {
		int w = bv_box(fr->lines[cursor_line].boxes, i)->width;
		w += bv_box(fr->lines[cursor_line].boxes, i)->sp;
		xposd += pango_units_to_double(w);
	}

	cx = fr->x + xposd;
	clow = fr->y + yposd;
	chigh = clow + line_height;

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

		draw_caret(cr, e->cursor_frame, e->cursor_line, e->cursor_box,
		               e->cursor_pos);

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


static void UNUSED tile_pixbuf(cairo_t *cr, GdkPixbuf *pb, int width, int height)
{
	int nx, ny, ix, iy, bgw, bgh;

	bgw = gdk_pixbuf_get_width(pb);
	bgh = gdk_pixbuf_get_height(pb);
	nx = width/bgw + 1;
	ny = height/bgh+ 1;
	for ( ix=0; ix<nx; ix++ ) {
	for ( iy=0; iy<ny; iy++ ) {
		gdk_cairo_set_source_pixbuf(cr, pb, ix*bgw, iy*bgh);
		cairo_rectangle(cr, ix*bgw, iy*bgh, width, height);
		cairo_fill(cr);
	}
	}
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, SCEditor *e)
{
	int width, height;

	/* Overall background */
	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_fill(cr);

	/* Contents */
	cairo_translate(cr, -e->h_scroll_pos, -e->scroll_pos);
	cairo_translate(cr, e->border_offs_x, e->border_offs_y);
	recursive_draw(e->top, cr, e->is, ISZ_EDITOR,
	               e->scroll_pos, e->scroll_pos + e->visible_height);

	/* Editing overlay */
	draw_overlay(cr, e);

	return FALSE;
}


static void fixup_cursor(SCEditor *e)
{
	struct frame *fr;
	struct wrap_line *sline;
	struct wrap_box *sbox;

	fr = e->cursor_frame;

	if ( e->cursor_line >= fr->n_lines ) {
		/* We find ourselves on a line which doesn't exist */
		e->cursor_line = fr->n_lines-1;
		e->cursor_box = bv_len(fr->lines[fr->n_lines-1].boxes)-1;
	}

	sline = &fr->lines[e->cursor_line];

	if ( e->cursor_box >= bv_len(sline->boxes) ) {

		/* We find ourselves in a box which doesn't exist */

		if ( e->cursor_line < fr->n_lines-1 ) {
			/* This isn't the last line, so go to the first box of
			 * the next line */
			e->cursor_line++;
			e->cursor_box = 0;
			sline = &e->cursor_frame->lines[e->cursor_line];
		} else {
			/* There are no more lines, so just go to the end */
			e->cursor_line = fr->n_lines-1;
			sline = &e->cursor_frame->lines[e->cursor_line];
			e->cursor_box = bv_len(sline->boxes)-1;
		}
	}

	assert(e->cursor_box < bv_len(sline->boxes));
	sbox = bv_box(sline->boxes, e->cursor_box);

	if ( e->cursor_pos > sbox->len_chars ) {
		advance_cursor(e);
	}
}


static void move_cursor(SCEditor *e, signed int x, signed int y)
{
	if ( x > 0 ) {
		advance_cursor(e);
	} else {
		move_cursor_back(e);
	}
}


void insert_scblock(SCBlock *scblock, SCEditor *e)
{
	int sln, sbx, sps;
	struct wrap_box *sbox;
	struct frame *fr = e->cursor_frame;

	if ( fr == NULL ) return;

	/* If this is, say, the top level frame, do nothing */
	if ( fr->boxes == NULL ) return;

	sln = e->cursor_line;
	sbx = e->cursor_box;
	sps = e->cursor_pos;
	sbox = bv_box(e->cursor_frame->lines[sln].boxes, sbx);

	sc_insert_block(sbox->scblock, sps+sbox->offs_char, scblock);

	fr->empty = 0;

	full_rerender(e); /* FIXME: No need for full */

	//fixup_cursor(e);
	//advance_cursor(e);
	//sc_editor_redraw(e);
}


static void shift_box_offsets(struct frame *fr, struct wrap_box *box, int n)
{
	int i;
	int sn = 0;

	for ( i=0; i<fr->boxes->n_boxes; i++ ) {
		if ( bv_box(fr->boxes, i) == box ) {
			sn = i+1;
			break;
		}
	}

	assert(sn > 0);  /* Lowest it can possibly be is 1 */

	for ( i=sn; i<fr->boxes->n_boxes; i++ ) {
		bv_box(fr->boxes, i)->offs_char += n;
	}
}


static void maybe_downgrade_box(struct wrap_box *box)
{
	if ( box->len_chars == 0 ) {

		printf("Downgrading box.\n");
		box->type = WRAP_BOX_NOTHING;
	}
}


static void split_boxes(struct wrap_box *sbox, PangoLogAttr *log_attrs,
                        int offs, int cursor_pos, struct boxvec *boxes,
                        PangoContext *pc)
{
	struct wrap_box *nbox;

	printf("Adding line break (new box) at pos %i\n", offs);
	printf("offset %i into box\n", cursor_pos);

	/* Add a new box containing the text after the break */
	nbox = calloc(1, sizeof(struct wrap_box));
	if ( nbox == NULL ) {
		fprintf(stderr, "Failed to allocate a text box.\n");
		return;
	}
	bv_add_after(boxes, sbox, nbox);
	nbox->type = WRAP_BOX_PANGO;
	nbox->space = sbox->space;
	nbox->len_chars = sbox->len_chars - cursor_pos;
	if ( nbox->len_chars == 0 ) {
		printf("WARNING! Zero-length box!\n");
	}
	nbox->offs_char = sbox->offs_char + cursor_pos;
	nbox->scblock = sbox->scblock;
	nbox->fontdesc = pango_font_description_copy(sbox->fontdesc);
	nbox->col[0] = sbox->col[0];
	nbox->col[1] = sbox->col[1];
	nbox->col[2] = sbox->col[2];
	nbox->col[3] = sbox->col[3];
	nbox->editable = sbox->editable;

	/* Shorten the text in the first box */
	sbox->len_chars = cursor_pos;
	if ( log_attrs[offs].is_expandable_space ) {
		sbox->space = WRAP_SPACE_INTERWORD;
		nbox->len_chars--;
		nbox->offs_char++;
		maybe_downgrade_box(nbox);
	} else if ( log_attrs[offs+1].is_mandatory_break ) {
		sbox->space = WRAP_SPACE_EOP;
		printf("New paragraph!\n");
		nbox->offs_char++;
		nbox->len_chars--;
		maybe_downgrade_box(nbox);
	} else {
		sbox->space = WRAP_SPACE_NONE;
		printf("two boxes.\n");
	}

	printf("boxes: <%i %i %i>[%i %i %i]\n",
	       sbox->offs_char, sbox->len_chars, sbox->n_segs,
	       nbox->offs_char, nbox->len_chars, nbox->n_segs);

	itemize_and_shape(nbox, pc);
	/* sbox will get done in just a moment */
}


static void fixup_line_breaks(struct wrap_box *sbox, struct boxvec *boxes,
                              int cursor_pos, PangoLanguage *lang,
                              PangoContext *pc)
{
	const char *text;
	size_t len_bytes;
	int len_chars;
	PangoLogAttr *log_attrs;
	int offs;

	/* Run pango_get_log_attrs on the entire SCBlock, to get good context */
	text = sc_block_contents(sbox->scblock);
	len_bytes = strlen(text);
	len_chars = g_utf8_strlen(text, -1);
	if ( len_chars <= 1 ) return;
	log_attrs = malloc((len_chars+1)*sizeof(PangoLogAttr));
	if ( log_attrs == NULL ) return;
	pango_get_log_attrs(text, len_bytes, -1, lang,
	                    log_attrs, len_chars+1);

	/* Take a peek at the situation near where we just typed */
	offs = sbox->offs_char + cursor_pos;
	if ( log_attrs[offs+1].is_line_break ) {

		/* If there just happens to be two boxes without a space
		 * between them, just tweak the space. */
		if ( (cursor_pos == sbox->len_chars-1)
		  && (sbox->space == WRAP_SPACE_NONE) )
		{
			printf("Easy case! :D\n");
			if ( log_attrs[offs].is_expandable_space ) {
				sbox->space = WRAP_SPACE_INTERWORD;
				sbox->len_chars--;
			} else if ( log_attrs[offs+1].is_mandatory_break ) {
				sbox->space = WRAP_SPACE_EOP;
				sbox->len_chars--;
			} else {
				printf("WTF space type?\n");
			}
		} else {
			split_boxes(sbox, log_attrs, offs, cursor_pos,
			            boxes, pc);
		}


	} else {
		if ( log_attrs[offs].is_expandable_space ) {
			printf("Space at end!\n");
			if ( sbox->space == WRAP_SPACE_NONE ) {
				sbox->space = WRAP_SPACE_INTERWORD;
			}
		}
	}

	free(log_attrs);
}


static void insert_text(char *t, SCEditor *e)
{
	int sln, sbx, sps;
	struct wrap_box *sbox;
	struct frame *fr = e->cursor_frame;

	printf("insert! ---------------------------------------------------\n");

	if ( fr == NULL ) return;

	/* If this is, say, the top level frame, do nothing */
	if ( fr->boxes == NULL ) return;

	sln = e->cursor_line;
	sbx = e->cursor_box;
	sps = e->cursor_pos;
	sbox = bv_box(e->cursor_frame->lines[sln].boxes, sbx);

	if ( sbox->type == WRAP_BOX_NOTHING ) {
		printf("Upgrading nothing box %p to Pango box\n", sbox);
		sbox->type = WRAP_BOX_PANGO;
		sbox->col[0] = fr->col[0];
		sbox->col[1] = fr->col[1];
		sbox->col[2] = fr->col[2];
		sbox->col[3] = fr->col[3];
		sbox->fontdesc = pango_font_description_copy(fr->fontdesc);
		sbox->segs = NULL;
	}

	cur_box_diag(e);
	printf("sps=%i, offs_char=%i\n", sps, sbox->offs_char);
	if ( sbox->type == WRAP_BOX_NOTHING ) {
		printf("Editing a nothing box!\n");
		return;
	}

	sc_insert_text(sbox->scblock, sps+sbox->offs_char, t);
	sbox->len_chars += 1;

	/* Tweak the offsets of all the subsequent boxes */
	shift_box_offsets(fr, sbox, 1);

	fixup_line_breaks(sbox, e->cursor_frame->boxes, e->cursor_pos,
	                  e->lang, e->pc);

	/* The box must be analysed by Pango again, because the segments
	 * might have changed */
	itemize_and_shape(sbox, e->pc);

	fr->empty = 0;

	wrap_contents(e->cursor_frame);
	update_size(e);
	fixup_cursor(e);
	printf("done! -----------------------------------------------------\n");

	advance_cursor(e);

	sc_editor_redraw(e);
}


static void do_backspace(struct frame *fr, SCEditor *e)
{
	int sln, sbx, sps;

	if ( fr == NULL ) return;

	/* If this is, say, the top level frame, do nothing */
	if ( fr->n_lines == 0 ) return;

	printf("Backspace! ------------------------------------------------\n");
	printf("sbox:\n");
	cur_box_diag(e);
	sln = e->cursor_line;
	sbx = e->cursor_box;
	sps = e->cursor_pos;
	struct wrap_box *sbox = bv_box(e->cursor_frame->lines[sln].boxes, sbx);

	move_cursor_back(e);
	printf("fbox:\n");
	cur_box_diag(e);

	/* Delete may cross wrap boxes and maybe SCBlock boundaries */
	struct wrap_line *fline = &e->cursor_frame->lines[e->cursor_line];
	struct wrap_box *fbox = bv_box(fline->boxes, e->cursor_box);

	if ( (fbox->scblock == NULL) || (sbox->scblock == NULL) ) return;
	sc_delete_text(fbox->scblock, e->cursor_pos+fbox->offs_char,
	               sbox->scblock, sps+sbox->offs_char);

	if ( (sps == 0) && (fbox->space == WRAP_SPACE_INTERWORD) ) {

		/* We are deleting an interword space.
		 * It's enough just to change the space type, and leave two
		 * boxes butted up with no space. */
		fbox->space = WRAP_SPACE_NONE;
		shift_box_offsets(fr, fbox, -1);
		itemize_and_shape(fbox, e->pc);

	} else if ( (sps == 0) && (fbox->space == WRAP_SPACE_NONE) ) {

		/* We are deleting across a box boundary, but there is no
		 * space to delete */
		if ( fbox->len_chars == 0 ) {
			printf("Deleting a zero-length box\n");

		} else {
			fbox->len_chars -= 1;
			shift_box_offsets(fr, fbox, -1);
			itemize_and_shape(fbox, e->pc);
		}

	} else if ( (sps == 0) && (fbox->space == WRAP_SPACE_EOP) ) {

		/* We are deleting the newline between paragraphs */
		fbox->space = WRAP_SPACE_NONE;
		shift_box_offsets(fr, fbox, -1);
		itemize_and_shape(fbox, e->pc);

	} else {

		sbox->len_chars -= 1;
		shift_box_offsets(fr, sbox, -1);
		if ( sbox->len_chars == 0 ) {

			if ( sbox->space == WRAP_SPACE_NONE ) {
				printf("deleting box.\n");
				bv_del(e->cursor_frame->boxes, sbox);
				free(sbox);
			} else {
				printf("downgrading box.\n");
				sbox->type = WRAP_BOX_NOTHING;
				sbox->width = 0;
			}

		} else {
			itemize_and_shape(sbox, e->pc);
		}

	}

	wrap_contents(e->cursor_frame);
	update_size(e);
	fixup_cursor(e);
	cur_box_diag(e);
	printf("done! -----------------------------------------------------\n");

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
	if ( (x<20.0) && (y>fr->h-20.0) ) {
		return CORNER_BL;
	}
	if ( (x>fr->w-20.0) && (y>fr->h-20.0) ) {
		return CORNER_BR;
	}

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


static struct wrap_box *cbox(struct frame *fr, int ln, int bn)
{
	return bv_box(fr->lines[ln].boxes, bn);;
}


static int callback_click(SCEditor *e, struct frame *fr, double x, double y)
{
	int ln, bn, pn;
	struct wrap_box *bx;

	find_cursor(fr, x, y, &ln, &bn, &pn);
	if ( (ln==0) && (bn==0) && (pn==0) ) return 0;
	/* FIXME: The above might be the case in a non-error situation.
	 * find_cursor() needs a better way of returning an error */
	bx = cbox(fr, ln, bn);
	if ( bx->type == WRAP_BOX_CALLBACK ) {
		return bx->click_func(x, y, bx->bvp, bx->vp);
	}
	return 0;
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 SCEditor *e)
{
	enum corner c;
	gdouble x, y;
	struct frame *clicked;

	x = event->x - e->border_offs_x;
	y = event->y - e->border_offs_y + e->scroll_pos;

	if ( within_frame(e->selection, x, y) ) {
		clicked = e->selection;
	} else {
		clicked = find_frame_at_position(e->top, x, y);
	}

	/* If the user clicked the currently selected frame, position cursor
	 * or possibly prepare for resize */
	if ( (e->selection != NULL) && (clicked == e->selection) ) {

		struct frame *fr;

		fr = e->selection;

		/* Within the resizing region? */
		c = which_corner(x, y, fr);
		if ( (c != CORNER_NONE) && (fr->resizable) ) {

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

			/* If this is a callback box, check to see if the owner
			 * is interested */
			if ( !callback_click(e, clicked,  x, y) ) {

				/* else position cursor and prepare for possible
				 * drag */

				e->cursor_frame = clicked;
				find_cursor(clicked, x-fr->x, y-fr->y,
				            &e->cursor_line, &e->cursor_box,
				            &e->cursor_pos);

				e->start_corner_x = event->x - e->border_offs_x;
				e->start_corner_y = event->y - e->border_offs_y;

				if ( fr->resizable ) {
					e->drag_status = DRAG_STATUS_COULD_DRAG;
					e->drag_reason = DRAG_REASON_MOVE;
				}

			}

		}

	} else if ( (clicked == NULL)
	         || ( !e->top_editable && (clicked == e->top) ) )
	{
		/* Clicked no object. Deselect old object and set up for
		 * (maybe) creating a new one. */
		e->selection = NULL;
		e->start_corner_x = event->x - e->border_offs_x;
		e->start_corner_y = event->y - e->border_offs_y;
		e->drag_status = DRAG_STATUS_COULD_DRAG;
		e->drag_reason = DRAG_REASON_CREATE;

	} else {

		/* Select new frame, no immediate dragging */
		e->drag_status = DRAG_STATUS_NONE;
		e->drag_reason = DRAG_REASON_NONE;
		e->selection = clicked;
		e->cursor_frame = clicked;
		find_cursor(clicked, x-clicked->x, y-clicked->y,
		            &e->cursor_line, &e->cursor_box,
		            &e->cursor_pos);

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
	y = event->y - e->border_offs_y;

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
	scblocks = sc_block_append_end(e->scblocks, "f", NULL, NULL);
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
		e->selection = fr;
		e->cursor_frame = fr;
		e->cursor_line = 0;
		e->cursor_box = 0;
		e->cursor_pos = 0;
		cur_box_diag(e);
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

	}

	e->drag_reason = DRAG_REASON_NONE;

	gtk_widget_grab_focus(GTK_WIDGET(da));
	sc_editor_redraw(e);
	return FALSE;
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
		sc_editor_remove_cursor(e);
		sc_editor_redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Left :
		if ( e->selection != NULL ) {
			move_cursor(e, -1, 0);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Right :
		if ( e->selection != NULL ) {
			move_cursor(e, +1, 0);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Up :
		if ( e->selection != NULL ) {
			move_cursor(e, 0, -1);
			sc_editor_redraw(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Down :
		if ( e->selection != NULL ) {
			move_cursor(e, 0, +1);
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
		cur_box_diag(e);

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
			e->selection = fr;
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
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(e->im_context),
	                                 win);
	gdk_window_set_accept_focus(win, TRUE);
	g_signal_connect(G_OBJECT(e->im_context), "commit",
			 G_CALLBACK(im_commit_sig), e);
	g_signal_connect(G_OBJECT(e), "key-press-event",
			 G_CALLBACK(key_press_sig), e);

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


void sc_editor_set_callbacks(SCEditor *e, SCCallbackList *cbl)
{
	if ( e->cbl != NULL ) sc_callback_list_free(e->cbl);
	e->cbl = cbl;
}


SCEditor *sc_editor_new(SCBlock *scblocks, SCBlock **stylesheets,
                        PangoLanguage *lang)
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
	sceditor->is = imagestore_new();
	sceditor->slidenum = 0;
	sceditor->min_border = 0.0;
	sceditor->top_editable = 0;
	sceditor->cbl = NULL;
	sceditor->scroll_pos = 0;
	sceditor->flow = 0;
	sceditor->lang = lang;

	sceditor->stylesheets = copy_ss_list(stylesheets);

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
