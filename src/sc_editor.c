/*
 * sc_editor.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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


G_DEFINE_TYPE(SCEditor, sc_editor, GTK_TYPE_DRAWING_AREA);


static void sc_editor_class_init(SCEditorClass *klass)
{
}


static void sc_editor_init(SCEditor *self)
{
}


void sc_editor_set_background(SCEditor *e, double r, double g, double b)
{
	e->bgcol[0] = r;
	e->bgcol[1] = g;
	e->bgcol[2] = b;
}


/* Update the view, once it's been edited in some way. */
static void rerender(SCEditor *e)
{
	if ( e->surface != NULL ) {
		cairo_surface_destroy(e->surface);
	}

	e->surface = render_sc(e->scblocks, e->w, e->h, e->log_w, e->log_h,
	                       e->stylesheets, e->cbl, e->is, ISZ_EDITOR,
	                       e->slidenum);
}


/* Force a redraw of the editor window */
static void redraw_editor(SCEditor *e)
{
	gint w, h;

	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));

	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


/* Force a redraw of the editor window */
void sc_editor_redraw(SCEditor *e)
{
	redraw_editor(e);
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
	box = &line->boxes[e->cursor_box];
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
				cb = line->n_boxes - 1;
			}

		} while ( !line->boxes[cb].editable );

		e->cursor_box = cb;
		box = &line->boxes[cb];
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

	struct wrap_box *sbox = &e->cursor_frame->lines[sln].boxes[sbx];

	printf("line/box/pos: [%i of %i]/[%i of %i]/[%i of %i]\n",
	       sln, fr->n_lines,
	       sbx, e->cursor_frame->lines[sln].n_boxes,
	       sps, sbox->len_chars);
	printf("box type is %i, space type is %i\n", sbox->type, sbox->space);
	if ( sbox->type == WRAP_BOX_NOTHING ) {
		printf("Warning: in a nothing box!\n");
	}
}


void advance_cursor(SCEditor *e)
{
	int advance = 0;
	signed int cp, cb, cl;
	struct wrap_line *line = &e->cursor_frame->lines[e->cursor_line];
	struct wrap_box *box = &line->boxes[e->cursor_box];

	cp = e->cursor_pos;
	cb = e->cursor_box;
	cl = e->cursor_line;

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

			if ( cb >= line->n_boxes ) {
				cl++;
				if ( cl >= e->cursor_frame->n_lines ) {
					/* Give up - could not move */
					return;
				}
				line = &e->cursor_frame->lines[cl];
				cb = 0;
				cp = 0;
			}

		} while ( !line->boxes[cb].editable );

		e->cursor_line = cl;
		e->cursor_box = cb;

	}
	e->cursor_pos = cp;
}


static gint destroy_sig(GtkWidget *window, SCEditor *e)
{
	if ( e->surface != NULL ) {
		cairo_surface_destroy(e->surface);
	}
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

	/* Locate the cursor in a "logical" and "geographical" sense */
	box = &fr->lines[cursor_line].boxes[cursor_box];
	get_cursor_pos(box, cursor_pos, &xposd, &yposd, &line_height);
	xposd += fr->pad_l;
	yposd += fr->pad_t;

	for ( i=0; i<cursor_line; i++ ) {
		yposd += pango_units_to_double(fr->lines[i].height);
	}

	for ( i=0; i<cursor_box; i++ ) {
		int w = fr->lines[cursor_line].boxes[i].width;
		w += fr->lines[cursor_line].boxes[i].sp;
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

		/* Draw resize handles */
		/* FIXME: Not if this frame can't be resized */
		draw_resize_handle(cr, x, y+h-20.0);
		draw_resize_handle(cr, x+w-20.0, y);
		draw_resize_handle(cr, x, y);
		draw_resize_handle(cr, x+w-20.0, y+h-20.0);

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


static void tile_pixbuf(cairo_t *cr, GdkPixbuf *pb, int width, int height)
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


static gboolean draw_sig(GtkWidget *da, cairo_t *cr,
                         SCEditor *e)
{
	int width, height;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	tile_pixbuf(cr, e->bg_pixbuf, width, height);
	cairo_set_source_rgba(cr, e->bgcol[0], e->bgcol[1], e->bgcol[2], 0.5);
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_fill(cr);

	/* Get the overall size */
	e->border_offs_x = rint((width - e->w)/2.0);
	e->border_offs_y = rint((height - e->h)/2.0);
	if ( e->border_offs_x < e->min_border ) {
		e->border_offs_x = e->min_border;
	}
	if ( e->border_offs_y < e->min_border ) {
		e->border_offs_y = e->min_border;
	}

	/* Draw the slide from the cache */
	if ( e->surface != NULL ) {
		cairo_set_source_surface(cr, e->surface,
		                         rint(e->border_offs_x),
		                         rint(e->border_offs_y));
		cairo_paint(cr);
	} else {
		fprintf(stderr, "Current slide not rendered yet!\n");
	}

	cairo_translate(cr, e->border_offs_x, e->border_offs_y);
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
		e->cursor_box = fr->lines[fr->n_lines-1].n_boxes-1;
	}

	sline = &fr->lines[e->cursor_line];

	if ( e->cursor_box >= sline->n_boxes ) {

		/* We find ourselves in a box which doesn't exist */

		if ( e->cursor_line > fr->n_lines-1 ) {
			/* This isn't the last line, so go to the first box of
			 * the next line */
			e->cursor_line++;
			e->cursor_box = 0;
			sline = &e->cursor_frame->lines[e->cursor_line];
		} else {
			/* There are no more lines, so just go to the end */
			e->cursor_line = fr->n_lines-1;
			sline = &e->cursor_frame->lines[e->cursor_line];
			e->cursor_box = sline->n_boxes-1;
		}
	}

	sbox = &sline->boxes[e->cursor_box];

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
	sbox = &e->cursor_frame->lines[sln].boxes[sbx];

	sc_insert_block(sbox->scblock, sps+sbox->offs_char, scblock);

	fr->empty = 0;

	rerender(e);

	fixup_cursor(e);
	advance_cursor(e);

	redraw_editor(e);
}


static void insert_text(char *t, SCEditor *e)
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
	sbox = &e->cursor_frame->lines[sln].boxes[sbx];

	sc_insert_text(sbox->scblock, sps+sbox->offs_char, t);

	fr->empty = 0;

	rerender(e);

	fixup_cursor(e);
	advance_cursor(e);

	redraw_editor(e);
}


static void do_backspace(struct frame *fr, SCEditor *e)
{
	int sln, sbx, sps;

	if ( fr == NULL ) return;

	/* If this is, say, the top level frame, do nothing */
	if ( fr->n_lines == 0 ) return;

	sln = e->cursor_line;
	sbx = e->cursor_box;
	sps = e->cursor_pos;
	struct wrap_box *sbox = &e->cursor_frame->lines[sln].boxes[sbx];

	move_cursor_back(e);

	/* Delete may cross wrap boxes and maybe SCBlock boundaries */
	struct wrap_line *fline = &e->cursor_frame->lines[e->cursor_line];
	struct wrap_box *fbox = &fline->boxes[e->cursor_box];

//	SCBlock *scbl = sbox->scblock;
//	do {
//		show_sc_blocks(scbl);
//		scbl = sc_block_next(scbl);
//	} while ( (scbl != fbox->scblock) && (scbl != NULL) );

	if ( (fbox->scblock == NULL) || (sbox->scblock == NULL) ) return;
	sc_delete_text(fbox->scblock, e->cursor_pos+fbox->offs_char,
	               sbox->scblock, sps+sbox->offs_char);

//	scbl = sbox->scblock;
//	do {
//		show_sc_blocks(scbl);
//		scbl = sc_block_next(scbl);
//	} while ( (scbl != fbox->scblock) && (scbl != NULL) );

	rerender(e);
	fixup_cursor(e);
	redraw_editor(e);
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


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 SCEditor *e)
{
	enum corner c;
	gdouble x, y;
	struct frame *clicked;

	x = event->x - e->border_offs_x;
	y = event->y - e->border_offs_y;

	if ( within_frame(e->selection, x, y) ) {
		clicked = e->selection;
	} else {
		clicked = find_frame_at_position(sc_block_frame(e->scblocks),
		                                 x, y);
	}

	/* If the user clicked the currently selected frame, position cursor
	 * or possibly prepare for resize */
	if ( (e->selection != NULL) && (clicked == e->selection) ) {

		struct frame *fr;

		fr = e->selection;

		/* Within the resizing region? */
		c = which_corner(x, y, fr);
		if ( c != CORNER_NONE ) {

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

			e->cursor_frame = clicked;
			find_cursor(clicked, x-fr->x, y-fr->y,
			            &e->cursor_line, &e->cursor_box,
			            &e->cursor_pos);

			e->start_corner_x = event->x - e->border_offs_x;
			e->start_corner_y = event->y - e->border_offs_y;
			e->drag_status = DRAG_STATUS_COULD_DRAG;
			e->drag_reason = DRAG_REASON_MOVE;

		}

	} else if ( (clicked == NULL)
	         || ( !e->top_editable
	           && (clicked == sc_block_frame(e->scblocks)) ) )
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

	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw_editor(e);
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
		redraw_editor(e);
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled by dnd_motion() */
		break;

		case DRAG_REASON_RESIZE :
		calculate_box_size(fr, e,  x, y);
		redraw_editor(e);
		break;

		case DRAG_REASON_MOVE :
		e->box_x = (fr->x - e->start_corner_x) + x;
		e->box_y = (fr->y - e->start_corner_y) + y;
		e->box_width = fr->w;
		e->box_height = fr->h;
		redraw_editor(e);
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

	parent = sc_block_frame(e->scblocks);

	if ( w < 0.0 ) {
		x += w;
		w = -w;
	}

	if ( h < 0.0 ) {
		y += h;
		h = -h;
	}

	fr = add_subframe(parent);

	/* Add to SC */
	fr->scblocks = sc_block_append_end(e->scblocks,
	                                   "f", NULL, NULL);
	sc_block_set_frame(fr->scblocks, fr);
	sc_block_append_inside(fr->scblocks, NULL, NULL, strdup(""));

	fr->x = x;
	fr->y = y;
	fr->w = w;
	fr->h = h;
	fr->is_image = 0;
	fr->empty = 1;

	update_geom(fr);

	return fr;
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

	rerender(e);
	redraw_editor(e);
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
		rerender(e);
		e->selection = fr;
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
	redraw_editor(e);
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
		e->selection = NULL;
		redraw_editor(e);
		claim = 1;
		break;

		case GDK_KEY_Left :
		if ( e->selection != NULL ) {
			move_cursor(e, -1, 0);
			redraw_editor(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Right :
		if ( e->selection != NULL ) {
			move_cursor(e, +1, 0);
			redraw_editor(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Up :
		if ( e->selection != NULL ) {
			move_cursor(e, 0, -1);
			redraw_editor(e);
		}
		claim = 1;
		break;

		case GDK_KEY_Down :
		if ( e->selection != NULL ) {
			move_cursor(e, 0, +1);
			redraw_editor(e);
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

		redraw_editor(e);

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
		e->import_height = (new_import_width *e->import_height)
		                     / e->w;
		e->import_width = new_import_width;
	}

	if ( e->import_height > e->h ) {

		int new_import_height;

		new_import_height = e->w/2;
		e->import_width = (new_import_height*e->import_width)
		                    / e->import_height;
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
			rerender(e);
			e->selection = fr;
			redraw_editor(e);
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
	rerender(e);

	return FALSE;
}


void sc_editor_set_scblock(SCEditor *e, SCBlock *scblocks)
{
	e->scblocks = scblocks;
	rerender(e);
	redraw_editor(e);
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
		rerender(e);
		redraw_editor(e);
	}
}


void sc_editor_set_logical_size(SCEditor *e, double w, double h)
{
	e->log_w = w;
	e->log_h = h;
	if ( gtk_widget_get_mapped(GTK_WIDGET(e)) ) {
		rerender(e);
		redraw_editor(e);
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


SCEditor *sc_editor_new(SCBlock *scblocks, SCBlock **stylesheets)
{
	SCEditor *sceditor;
	GtkTargetEntry targets[1];
	GError *err;

	sceditor = g_object_new(SC_TYPE_EDITOR, NULL);

	sceditor->scblocks = scblocks;
	sceditor->surface = NULL;
	sceditor->w = 100;
	sceditor->h = 100;
	sceditor->log_w = 100;
	sceditor->log_h = 100;
	sceditor->is = imagestore_new();
	sceditor->slidenum = 0;
	sceditor->min_border = 0.0;
	sceditor->top_editable = 0;
	sceditor->cbl = NULL;

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
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	g_signal_connect(G_OBJECT(sceditor), "draw",
			 G_CALLBACK(draw_sig), sceditor);

	gtk_widget_grab_focus(GTK_WIDGET(sceditor));

	gtk_widget_show(GTK_WIDGET(sceditor));

	return sceditor;
}
