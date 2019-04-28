/*
 * gtkslideview.c
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
#include <libintl.h>
#define _(x) gettext(x)

#include <narrative.h>
#include <slide_render_cairo.h>
#include <stylesheet.h>

#include "gtkslideview.h"
#include "slide_priv.h"


G_DEFINE_TYPE_WITH_CODE(GtkSlideView, gtk_slide_view, GTK_TYPE_DRAWING_AREA,
                        NULL)

static int resizable(SlideItem *item)
{
	if ( item->type == SLIDE_ITEM_TEXT ) return 1;
	if ( item->type == SLIDE_ITEM_IMAGE ) return 1;
	return 0;
}

static gboolean resize_sig(GtkWidget *widget, GdkEventConfigure *event,
                           GtkSlideView *e)
{
	double sx, sy;
	double aw, ah;
	double log_w, log_h;
	Stylesheet *ss;

	ss = narrative_get_stylesheet(e->n);
	if ( slide_get_logical_size(e->slide, ss, &log_w, &log_h) ) {
		fprintf(stderr, "Failed to get logical size\n");
		return FALSE;
	}

	e->w = event->width;
	e->h = event->height;
	sx = (double)e->w / log_w;
	sy = (double)e->h / log_h;
	e->view_scale = (sx < sy) ? sx : sy;

	/* Actual size (in device units) */
	aw = e->view_scale * log_w;
	ah = e->view_scale * log_h;

	e->border_offs_x = (event->width - aw)/2.0;
	e->border_offs_y = (event->height - ah)/2.0;

	e->visible_height = event->height;
	e->visible_width = event->width;

	//update_size(e);

	return FALSE;
}


static void emit_change_sig(GtkSlideView *e)
{
	g_signal_emit_by_name(e, "changed");
}


static void gtk_slide_view_class_init(GtkSlideViewClass *klass)
{
	g_signal_new("changed", GTK_TYPE_SLIDE_VIEW, G_SIGNAL_RUN_LAST, 0,
	             NULL, NULL, NULL, G_TYPE_NONE, 0);
}


static void gtk_slide_view_init(GtkSlideView *e)
{
}


static void redraw(GtkSlideView *e)
{
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));
	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


static gint destroy_sig(GtkWidget *window, GtkSlideView *e)
{
	return 0;
}


static void draw_editing_box(cairo_t *cr, SlideItem *item,
                             Stylesheet *stylesheet, double slide_w, double slide_h,
                             double xmin, double ymin, double width, double height)
{
	const double dash[] = {2.0, 2.0};
	double ptot_w, ptot_h;
	double pad_l, pad_r, pad_t, pad_b;

	slide_item_get_padding(item, stylesheet, &pad_l, &pad_r, &pad_t, &pad_b,
	                       slide_w, slide_h);

	cairo_new_path(cr);
	cairo_rectangle(cr, xmin, ymin, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.69, 1.0);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);

	cairo_new_path(cr);
	ptot_w = pad_l + pad_r;
	ptot_h = pad_t + pad_b;
	cairo_rectangle(cr, xmin+pad_l, ymin+pad_t, width-ptot_w, height-ptot_h);
	cairo_set_dash(cr, dash, 2, 0.0);
	cairo_set_source_rgba(cr, 0.8, 0.8, 1.0, 0.5);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	cairo_set_dash(cr, NULL, 0, 0.0);
}


static void draw_resize_handle(cairo_t *cr, double x, double y)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, 20.0, 20.0);
	cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
	cairo_fill(cr);
}


static size_t pos_trail_to_offset(SlideItem *item, int para,
                                  size_t offs, int trail)
{
	glong char_offs;
	char *ptr;

	char_offs = g_utf8_pointer_to_offset(item->paras[para].text,
	                                     item->paras[para].text+offs);
	char_offs += trail;
	ptr = g_utf8_offset_to_pointer(item->paras[para].text, char_offs);
	return ptr - item->paras[para].text;
}


static double para_top(SlideItem *item, int pnum)
{
	int i;
	double py = 0.0;
	for ( i=0; i<pnum; i++ ) {
		PangoRectangle rect;
		pango_layout_get_extents(item->paras[i].layout, NULL, &rect);
		py += pango_units_to_double(rect.height);
	}
	return py;
}


static int get_cursor_pos(SlideItem *item, Stylesheet *stylesheet,
                          struct slide_pos cpos, double slide_w, double slide_h,
                          double *x, double *y, double *h)
{
	size_t offs;
	PangoRectangle rect;
	double padl, padr, padt, padb;

	if ( item->paras[cpos.para].layout == NULL ) {
		fprintf(stderr, "get_cursor_pos: No layout\n");
		return 1;
	}

	slide_item_get_padding(item, stylesheet, &padl, &padr, &padt, &padb,
	                       slide_w, slide_h);

	offs = pos_trail_to_offset(item, cpos.para, cpos.pos, cpos.trail);
	pango_layout_get_cursor_pos(item->paras[cpos.para].layout, offs, &rect, NULL);
	*x = pango_units_to_double(rect.x) + padl;
	*y = pango_units_to_double(rect.y) + para_top(item, cpos.para) + padt;
	*h = pango_units_to_double(rect.height);
	return 0;
}


static void draw_caret(cairo_t *cr, Stylesheet *stylesheet,
                       SlideItem *item, struct slide_pos cpos,
                       double frx, double fry, double slide_w, double slide_h)
{
	double cx, clow, chigh, h;
	const double t = 1.8;

	if ( get_cursor_pos(item, stylesheet, cpos, slide_w, slide_h,
	                    &cx, &clow, &h) ) return;

	cx += frx;
	clow += fry;
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


static void draw_overlay(cairo_t *cr, GtkSlideView *e)
{
	if ( e->cursor_frame != NULL ) {

		double x, y, w, h;
		double slide_w, slide_h;
		Stylesheet *stylesheet;

		stylesheet = narrative_get_stylesheet(e->n);
		slide_get_logical_size(e->slide, stylesheet, &slide_w, &slide_h);
		slide_item_get_geom(e->cursor_frame, stylesheet, &x, &y, &w, &h,
		                    slide_w, slide_h);
		draw_editing_box(cr, e->cursor_frame, stylesheet, slide_w, slide_h,
		                 x, y, w, h);

		if ( resizable(e->cursor_frame) ) {
			/* Draw resize handles */
			draw_resize_handle(cr, x, y+h-20.0);
			draw_resize_handle(cr, x+w-20.0, y);
			draw_resize_handle(cr, x, y);
			draw_resize_handle(cr, x+w-20.0, y+h-20.0);
		}

		if ( e->cursor_frame->type != SLIDE_ITEM_IMAGE ) {
			draw_caret(cr, stylesheet, e->cursor_frame, e->cpos, x, y,
			           slide_w, slide_h);
		}

	}

	if ( e->drag_status == DRAG_STATUS_DRAGGING ) {

		if ( (e->drag_reason == DRAG_REASON_CREATE)
		  || (e->drag_reason == DRAG_REASON_IMPORT) )
		{
			cairo_new_path(cr);
			cairo_rectangle(cr, e->start_corner_x, e->start_corner_y,
			                    e->drag_corner_x - e->start_corner_x,
			                    e->drag_corner_y - e->start_corner_y);
			cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
			cairo_set_line_width(cr, 0.5);
			cairo_stroke(cr);
		}

		if ( (e->drag_reason == DRAG_REASON_RESIZE)
		  || (e->drag_reason == DRAG_REASON_MOVE) )
		{
			cairo_new_path(cr);
			cairo_rectangle(cr, e->box_x, e->box_y,
			                    e->box_width, e->box_height);
			cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
			cairo_set_line_width(cr, 0.5);
			cairo_stroke(cr);
		}

	}
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, GtkSlideView *e)
{
	PangoContext *pc;

	/* Ultimate background */
	if ( e->bg_pixbuf != NULL ) {
		gdk_cairo_set_source_pixbuf(cr, e->bg_pixbuf, 0.0, 0.0);
		cairo_pattern_t *patt = cairo_get_source(cr);
		cairo_pattern_set_extend(patt, CAIRO_EXTEND_REPEAT);
		cairo_paint(cr);
	} else {
		cairo_set_source_rgba(cr, 0.8, 0.8, 1.0, 1.0);
		cairo_paint(cr);
	}

	cairo_translate(cr, e->border_offs_x, e->border_offs_y);
	cairo_translate(cr, -e->h_scroll_pos, -e->v_scroll_pos);
	cairo_scale(cr, e->view_scale, e->view_scale);

	/* Contents */
	pc = pango_cairo_create_context(cr);
	slide_render_cairo(e->slide, cr, narrative_get_imagestore(e->n),
	                   narrative_get_stylesheet(e->n),
	                   narrative_get_slide_number_for_slide(e->n, e->slide),
	                   pango_language_get_default(), pc,
	                   e->cursor_frame, e->sel_start, e->sel_end);
	g_object_unref(pc);

	/* Editing overlay */
	draw_overlay(cr, e);

	return FALSE;
}


static int within_frame(SlideItem *item, Stylesheet *ss,
                        double slide_w, double slide_h,
                        double xp, double yp)
{
	double x, y, w, h;

	slide_item_get_geom(item, ss, &x, &y, &w, &h, slide_w, slide_h);

	if ( xp < x ) return 0;
	if ( yp < y ) return 0;
	if ( xp > x + w ) return 0;
	if ( yp > y + h ) return 0;
	return 1;
}


static SlideItem *find_frame_at_position(Slide *s, Stylesheet *ss,
                                                 double slide_w, double slide_h,
                                                 double x, double y)
{
	int i;
	for ( i=0; i<s->n_items; i++ ) {
		if ( within_frame(&s->items[i], ss, slide_w, slide_h, x, y) ) {
			return &s->items[i];
		}
	}
	return NULL;
}


static enum drag_corner which_corner(double xp, double yp,
                                     double frx, double fry, double frw, double frh)
{
	double x, y;  /* Relative to object position */

	x = xp - frx;
	y = yp - fry;

	if ( x < 0.0 ) return CORNER_NONE;
	if ( y < 0.0 ) return CORNER_NONE;
	if ( x > frw ) return CORNER_NONE;
	if ( y > frh ) return CORNER_NONE;

	/* Top left? */
	if ( (x<20.0) && (y<20.0) ) return CORNER_TL;
	if ( (x>frw-20.0) && (y<20.0) ) return CORNER_TR;
	if ( (x<20.0) && (y>frh-20.0) ) return CORNER_BL;
	if ( (x>frw-20.0) && (y>frh-20.0) ) return CORNER_BR;

	return CORNER_NONE;
}


static void calculate_box_size(double frx, double fry, double frw, double frh,
                               GtkSlideView *e, int preserve_aspect,
                               double x, double y)
{
	double ddx, ddy, dlen, mult;
	double vx, vy, dbx, dby;

	ddx = x - e->start_corner_x;
	ddy = y - e->start_corner_y;

	if ( !preserve_aspect ) {

		switch ( e->drag_corner ) {

			case CORNER_BR :
			e->box_x = frx;
			e->box_y = fry;
			e->box_width = frw + ddx;
			e->box_height = frh + ddy;
			break;

			case CORNER_BL :
			e->box_x = frx + ddx;
			e->box_y = fry;
			e->box_width = frw - ddx;
			e->box_height = frh + ddy;
			break;

			case CORNER_TL :
			e->box_x = frx + ddx;
			e->box_y = fry + ddy;
			e->box_width = frw - ddx;
			e->box_height = frh - ddy;
			break;

			case CORNER_TR :
			e->box_x = frx;
			e->box_y = fry + ddy;
			e->box_width = frw + ddx;
			e->box_height = frh - ddy;
			break;

			case CORNER_NONE :
			break;

		}
		return;


	}

	switch ( e->drag_corner ) {

		case CORNER_BR :
		vx = frw;
		vy = frh;
		break;

		case CORNER_BL :
		vx = -frw;
		vy = frh;
		break;

		case CORNER_TL :
		vx = -frw;
		vy = -frh;
		break;

		case CORNER_TR :
		vx = frw;
		vy = -frh;
		break;

		case CORNER_NONE :
		default:
		vx = 0.0;
		vy = 0.0;
		break;

	}

	dlen = (ddx*vx + ddy*vy) / e->diagonal_length;
	mult = (dlen+e->diagonal_length) / e->diagonal_length;

	e->box_width = frw * mult;
	e->box_height = frh * mult;
	dbx = e->box_width - frw;
	dby = e->box_height - frh;

	if ( e->box_width < 40.0 ) {
		mult = 40.0 / frw;
	}
	if ( e->box_height < 40.0 ) {
		mult = 40.0 / frh;
	}
	e->box_width = frw * mult;
	e->box_height = frh * mult;
	dbx = e->box_width - frw;
	dby = e->box_height - frh;

	switch ( e->drag_corner ) {

		case CORNER_BR :
		e->box_x = frx;
		e->box_y = fry;
		break;

		case CORNER_BL :
		e->box_x = frx - dbx;
		e->box_y = fry;
		break;

		case CORNER_TL :
		e->box_x = frx - dbx;
		e->box_y = fry - dby;
		break;

		case CORNER_TR :
		e->box_x = frx;
		e->box_y = fry - dby;
		break;

		case CORNER_NONE :
		break;

	}
}


static int is_text(enum slide_item_type type)
{
	if ( type == SLIDE_ITEM_IMAGE ) return 0;
	return 1;
}


static int find_cursor(SlideItem *item, Stylesheet *stylesheet,
                       double x, double y, struct slide_pos *pos,
                       double slide_w, double slide_h)
{
	double cur_y = 0.0;
	double top;
	int i = 0;
	double padl, padr, padt, padb;

	if ( !is_text(item->type) ) {
		pos->para = 0;
		pos->pos = 0;
		pos->trail = 0;
		return 0;
	}

	slide_item_get_padding(item, stylesheet, &padl, &padr, &padt, &padb,
	                       slide_w, slide_h);
	x -= padl;
	y -= padt;

	do {
		PangoRectangle rect;
		pango_layout_get_extents(item->paras[i++].layout, NULL, &rect);
		top = cur_y;
		cur_y += pango_units_to_double(rect.height);
	} while ( (cur_y < y) && (i<item->n_paras) );

	pos->para = i-1;

	pango_layout_xy_to_index(item->paras[i-1].layout,
	                         pango_units_from_double(x),
	                         pango_units_from_double(y - top),
	                         &pos->pos, &pos->trail);
	return 0;
}


static void unset_selection(GtkSlideView *e)
{
	e->sel_start.para = 0;
	e->sel_start.pos = 0;
	e->sel_start.trail = 0;
	e->sel_end.para = 0;
	e->sel_end.pos = 0;
	e->sel_end.trail = 0;
}


static void do_resize(GtkSlideView *e, double x, double y, double w, double h)
{
	double slide_w, slide_h;
	Stylesheet *stylesheet;

	assert(e->cursor_frame != NULL);

	stylesheet = narrative_get_stylesheet(e->n);
	slide_get_logical_size(e->slide, stylesheet, &slide_w, &slide_h);

	if ( w < 0.0 ) {
		w = -w;
		x -= w;
	}

	if ( h < 0.0 ) {
		h = -h;
		y -= h;
	}

	/* If any of the units are fractional, turn the absolute values back
	 * into fractional values */
	if ( e->cursor_frame->geom.x.unit == LENGTH_FRAC ) {
		e->cursor_frame->geom.x.len = x / slide_w;
	} else {
		e->cursor_frame->geom.x.len = x;
	}

	if ( e->cursor_frame->geom.y.unit == LENGTH_FRAC ) {
		e->cursor_frame->geom.y.len = y / slide_h;
	} else {
		e->cursor_frame->geom.y.len = y;
	}

	if ( e->cursor_frame->geom.w.unit == LENGTH_FRAC ) {
		e->cursor_frame->geom.w.len = w / slide_w;
	} else {
		e->cursor_frame->geom.w.len = w;
	}

	if ( e->cursor_frame->geom.h.unit == LENGTH_FRAC ) {
		e->cursor_frame->geom.h.len = h / slide_h;
	} else {
		e->cursor_frame->geom.h.len = h;
	}

	redraw(e);
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 GtkSlideView *e)
{
	enum drag_corner c;
	gdouble x, y;
	Stylesheet *stylesheet;
	SlideItem *clicked;
	int shift;
	double slide_w, slide_h;
	double frx, fry, frw, frh;

	stylesheet = narrative_get_stylesheet(e->n);
	slide_get_logical_size(e->slide, stylesheet, &slide_w, &slide_h);

	x = event->x - e->border_offs_x + e->h_scroll_pos;
	y = event->y - e->border_offs_y + e->v_scroll_pos;
	x /= e->view_scale;
	y /= e->view_scale;
	shift = event->state & GDK_SHIFT_MASK;

	clicked = find_frame_at_position(e->slide, stylesheet,
	                                 slide_w, slide_h, x, y);

	if ( clicked != NULL ) {
		slide_item_get_geom(clicked, stylesheet, &frx, &fry, &frw, &frh,
		                    slide_w, slide_h);
	}

	/* Clicked within the currently selected frame
	 *   -> resize, move or select text */
	if ( (e->cursor_frame != NULL) && (clicked == e->cursor_frame) ) {

		/* Within the resizing region? */
		c = which_corner(x, y, frx, fry, frw, frh);
		if ( (c != CORNER_NONE) && resizable(e->cursor_frame) && shift ) {

			e->drag_reason = DRAG_REASON_RESIZE;
			e->drag_corner = c;

			e->start_corner_x = x;
			e->start_corner_y = y;
			e->diagonal_length = pow(frw, 2.0);
			e->diagonal_length += pow(frh, 2.0);
			e->diagonal_length = sqrt(e->diagonal_length);

			calculate_box_size(frx, fry, frw, frh, e,
			                   (e->cursor_frame->type == SLIDE_ITEM_IMAGE),
			                   x, y);

			e->drag_status = DRAG_STATUS_COULD_DRAG;
			e->drag_reason = DRAG_REASON_RESIZE;

		} else {

			/* Position cursor and prepare for possible drag */
			e->cursor_frame = clicked;
			find_cursor(clicked, stylesheet, x-frx, y-fry, &e->cpos,
			            slide_w, slide_h);

			e->start_corner_x = x;
			e->start_corner_y = y;

			if ( resizable(clicked) && shift ) {
				e->drag_status = DRAG_STATUS_COULD_DRAG;
				e->drag_reason = DRAG_REASON_MOVE;
			} else {
				e->drag_status = DRAG_STATUS_COULD_DRAG;
				e->drag_reason = DRAG_REASON_TEXTSEL;
				find_cursor(clicked, stylesheet, x-frx, y-fry,
				            &e->sel_start, slide_w, slide_h);
				e->sel_end = e->sel_start;
			}

		}

	} else if ( clicked == NULL ) {

		/* Clicked no object. Deselect old object.
		 * If shift held, set up for creating a new one. */
		e->cursor_frame = NULL;
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
		find_cursor(clicked, stylesheet, x-frx, y-fry, &e->sel_start,
		            slide_w, slide_h);
		find_cursor(clicked, stylesheet, x-frx, y-fry, &e->sel_end,
		            slide_w, slide_h);
		e->cursor_frame = clicked;
		find_cursor(clicked, stylesheet, x-frx, y-fry, &e->cpos,
		            slide_w, slide_h);

	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw(e);
	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event, GtkSlideView *e)
{
	gdouble x, y;
	double frx, fry, frw, frh;
	Stylesheet *stylesheet;
	double slide_w, slide_h;

	x = event->x - e->border_offs_x + e->h_scroll_pos;
	y = event->y - e->border_offs_y + e->v_scroll_pos;
	x /= e->view_scale;
	y /= e->view_scale;

	stylesheet = narrative_get_stylesheet(e->n);
	slide_get_logical_size(e->slide, stylesheet, &slide_w, &slide_h);

	if ( e->drag_status == DRAG_STATUS_COULD_DRAG ) {

		/* We just got a motion signal, and the status was "could drag",
		 * therefore the drag has started. */
		e->drag_status = DRAG_STATUS_DRAGGING;

	}

	if ( e->cursor_frame != NULL ) {
		slide_item_get_geom(e->cursor_frame, stylesheet,
		                    &frx, &fry, &frw, &frh, slide_w, slide_h);
	}

	switch ( e->drag_reason ) {

		case DRAG_REASON_NONE :
		break;

		case DRAG_REASON_CREATE :
		e->drag_corner_x = x;
		e->drag_corner_y = y;
		redraw(e);
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled by dnd_motion() */
		break;

		case DRAG_REASON_RESIZE :
		calculate_box_size(frx, fry, frw, frh, e,
		                   (e->cursor_frame->type == SLIDE_ITEM_IMAGE),
		                   x, y);
		redraw(e);
		break;

		case DRAG_REASON_MOVE :
		e->box_x = (frx - e->start_corner_x) + x;
		e->box_y = (fry - e->start_corner_y) + y;
		e->box_width = frw;
		e->box_height = frh;
		redraw(e);
		break;

		case DRAG_REASON_TEXTSEL :
		find_cursor(e->cursor_frame, stylesheet, x-frx, y-fry, &e->sel_end, slide_w, slide_h);
		e->cpos = e->sel_end;
		redraw(e);
		break;

	}

	gdk_event_request_motions(event);
	return FALSE;
}


static SlideItem *create_image(GtkSlideView *e, const char *filename,
                               double cx, double cy, double w, double h)
{
	struct frame_geom geom;
	char *fn = strdup(filename);
	if ( fn == NULL ) return NULL;
	geom.x.len = cx;  geom.x.unit = LENGTH_UNIT;
	geom.y.len = cy;  geom.y.unit = LENGTH_UNIT;
	geom.w.len = w;   geom.w.unit = LENGTH_UNIT;
	geom.h.len = h;   geom.h.unit = LENGTH_UNIT;
	return slide_add_image(e->slide, fn, geom);
}


static SlideItem *create_frame(GtkSlideView *e, double cx, double cy,
                               double w, double h)
{
	struct frame_geom geom;
	char *text;

	text = strdup("");
	if ( text == NULL ) return NULL;

	if ( w < 0.0 ) {
		cx += w;
		w = -w;
	}

	if ( h < 0.0 ) {
		cy += h;
		h = -h;
	}

	geom.x.len = cx;  geom.x.unit = LENGTH_UNIT;
	geom.y.len = cy;  geom.y.unit = LENGTH_UNIT;
	geom.w.len = w;   geom.w.unit = LENGTH_UNIT;
	geom.h.len = h;   geom.h.unit = LENGTH_UNIT;
	return slide_add_text(e->slide, &text, 1, geom, ALIGN_INHERIT);
}


static gboolean button_release_sig(GtkWidget *da, GdkEventButton *event,
                                   GtkSlideView *e)
{
	gdouble x, y;
	SlideItem *fr;

	x = event->x - e->border_offs_x + e->h_scroll_pos;
	y = event->y - e->border_offs_y + e->v_scroll_pos;
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
			e->cursor_frame = fr;
			e->cpos.para = 0;
			e->cpos.pos = 0;
			e->cpos.trail = 0;
			unset_selection(e);
		} else {
			fprintf(stderr, _("Failed to create frame!\n"));
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
	redraw(e);
	return FALSE;
}


static size_t end_offset_of_para(SlideItem *item, int pnum)
{
	assert(pnum >= 0);
	if ( !is_text(item->type) ) return 0;
	return strlen(item->paras[pnum].text);
}


static void cursor_moveh(GtkSlideView *e, struct slide_pos *cp, signed int dir)
{
	int np = cp->pos;

	if ( !is_text(e->cursor_frame->type) ) return;
	if ( e->cursor_frame->paras[e->cpos.para].layout == NULL ) return;
	unset_selection(e);

	pango_layout_move_cursor_visually(e->cursor_frame->paras[e->cpos.para].layout,
	                                  1, cp->pos, cp->trail, dir,
	                                  &np, &cp->trail);

	if ( np == -1 ) {
		if ( cp->para > 0 ) {
			size_t end_offs;
			cp->para--;
			end_offs = end_offset_of_para(e->cursor_frame, cp->para);
			if ( end_offs > 0 ) {
				cp->pos = end_offs - 1;
				cp->trail = 1;
			} else {
				/* Jumping into an empty paragraph */
				cp->pos = 0;
				cp->trail = 0;
			}
			return;
		} else {
			/* Can't move any further */
			return;
		}
	}

	if ( np == G_MAXINT ) {
		if ( cp->para < e->cursor_frame->n_paras-1 ) {
			cp->para++;
			cp->pos = 0;
			cp->trail = 0;
			return;
		} else {
			/* Can't move any further */
			cp->trail = 1;
			return;
		}
	}

	cp->pos = np;
}


static int slide_positions_equal(struct slide_pos a, struct slide_pos b)
{
	if ( a.para != b.para ) return 0;
	if ( a.pos != b.pos ) return 0;
	if ( a.trail != b.trail ) return 0;
	return 1;
}


static void sort_slide_positions(struct slide_pos *a, struct slide_pos *b)
{
	if ( a->para > b->para ) {
		size_t tpos;
		int tpara, ttrail;
		tpara = b->para;   tpos = b->pos;  ttrail = b->trail;
		b->para = a->para;  b->pos = a->pos;  b->trail = a->trail;
		a->para = tpara;    a->pos = tpos;    a->trail = ttrail;
	}

	if ( (a->para == b->para) && (a->pos > b->pos) )
	{
		size_t tpos = b->pos;
		int ttrail = b->trail;
		b->pos = a->pos;  b->trail = a->trail;
		a->pos = tpos;    a->trail = ttrail;
	}
}


static void do_backspace(GtkSlideView *e, signed int dir)
{
	struct slide_pos p1, p2;
	size_t o1, o2;

	if ( !slide_positions_equal(e->sel_start, e->sel_end) ) {

		/* Block delete */
		p1 = e->sel_start;
		p2 = e->sel_end;

	} else {

		/* Delete one character, as represented visually */
		p2 = e->cpos;
		p1 = p2;
		cursor_moveh(e, &p1, dir);
	}

	sort_slide_positions(&p1, &p2);
	o1 = pos_trail_to_offset(e->cursor_frame, p1.para, p1.pos, p1.trail);
	o2 = pos_trail_to_offset(e->cursor_frame, p2.para, p2.pos, p2.trail);
	slide_item_delete_text(e->cursor_frame, p1.para, o1, p2.para, o2);
	e->cpos = p1;
	unset_selection(e);

	pango_layout_set_text(e->cursor_frame->paras[e->cpos.para].layout,
	                      e->cursor_frame->paras[e->cpos.para].text, -1);

	emit_change_sig(e);
	redraw(e);
}


static void insert_text_in_paragraph(SlideItem *item, int para,
                                     size_t offs, char *t)
{
	char *n = malloc(strlen(t) + strlen(item->paras[para].text) + 1);
	if ( n == NULL ) return;
	strncpy(n, item->paras[para].text, offs);
	n[offs] = '\0';
	strcat(n, t);
	strcat(n, item->paras[para].text+offs);
	free(item->paras[para].text);
	item->paras[para].text = n;
}


static void insert_text(char *t, GtkSlideView *e)
{
	size_t off;

	if ( e->cursor_frame == NULL ) return;
	if ( !is_text(e->cursor_frame->type) ) return;

	if ( !slide_positions_equal(e->sel_start, e->sel_end) ) {
		do_backspace(e, 0);
	}
	unset_selection(e);

	if ( strcmp(t, "\n") == 0 ) {
		off = pos_trail_to_offset(e->cursor_frame, e->cpos.para,
		                          e->cpos.pos, e->cpos.trail);
		slide_item_split_text_paragraph(e->cursor_frame, e->cpos.para, off);
		e->cpos.para++;
		e->cpos.pos = 0;
		e->cpos.trail = 0;
		emit_change_sig(e);
		redraw(e);
		return;
	}

	off = pos_trail_to_offset(e->cursor_frame, e->cpos.para,
	                          e->cpos.pos, e->cpos.trail);
	insert_text_in_paragraph(e->cursor_frame, e->cpos.para, off, t);
	pango_layout_set_text(e->cursor_frame->paras[e->cpos.para].layout,
	                      e->cursor_frame->paras[e->cpos.para].text, -1);
	cursor_moveh(e, &e->cpos, +1);
	emit_change_sig(e);
	redraw(e);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              GtkSlideView *e)
{
	insert_text(str, e);
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              GtkSlideView *e)
{
	gboolean r;
	int claim = 0;

	/* Throw the event to the IM context and let it sort things out */
	r = gtk_im_context_filter_keypress(GTK_IM_CONTEXT(e->im_context),
		                           event);
	if ( r ) return FALSE;  /* IM ate it */

	switch ( event->keyval ) {

		case GDK_KEY_Left :
		cursor_moveh(e, &e->cpos, -1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Right :
		cursor_moveh(e, &e->cpos, +1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Up :
		cursor_moveh(e, &e->cpos, -1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Down :
		cursor_moveh(e, &e->cpos, +1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Return :
		im_commit_sig(NULL, "\n", e);
		claim = 1;
		break;

		case GDK_KEY_BackSpace :
		do_backspace(e, -1);
		claim = 1;
		break;

		case GDK_KEY_Delete :
		do_backspace(e, +1);
		claim = 1;
		break;

	}

	if ( claim ) return TRUE;
	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, GtkSlideView *e)
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

		redraw(e);

	}

	return TRUE;
}


static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                         gint x, gint y, guint time, GtkSlideView *e)
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


/* Scale the image down if it's a silly size */
static void check_import_size(GtkSlideView *e)
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


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, GtkSlideView *e)
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

			int w, h;

			gtk_drag_finish(drag_context, TRUE, FALSE, time);
			chomp(filename);

			w = e->drag_corner_x - e->start_corner_x;
			h = e->drag_corner_y - e->start_corner_y;

			create_image(e, filename,
			             e->start_corner_x, e->start_corner_y,
			             w, h);
			free(filename);
			redraw(e);

		} else {

			gtk_drag_finish(drag_context, FALSE, FALSE, time);

		}

	}
}


static void dnd_leave(GtkWidget *widget, GdkDragContext *drag_context,
                      guint time, GtkSlideView *e)
{
	if ( e->drag_highlight ) {
		gtk_drag_unhighlight(widget);
	}
	e->have_drag_data = 0;
	e->drag_highlight = 0;
	e->drag_status = DRAG_STATUS_NONE;
	e->drag_reason = DRAG_REASON_NONE;
}


static gint realise_sig(GtkWidget *da, GtkSlideView *e)
{
	GdkWindow *win;

	/* Keyboard and input method stuff */
	e->im_context = gtk_im_multicontext_new();
	win = gtk_widget_get_window(GTK_WIDGET(e));
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(e->im_context), win);
	gdk_window_set_accept_focus(win, TRUE);
	g_signal_connect(G_OBJECT(e->im_context), "commit", G_CALLBACK(im_commit_sig), e);
	g_signal_connect(G_OBJECT(e), "key-press-event", G_CALLBACK(key_press_sig), e);

	return FALSE;
}


void gtk_slide_view_set_scale(GtkSlideView *e, double scale)
{
	e->view_scale = 1.0;
}


void gtk_slide_view_set_slide(GtkWidget *widget, Slide *slide)
{
	GtkSlideView *e = GTK_SLIDE_VIEW(widget);
	e->slide = slide;
	unset_selection(e);
	e->cursor_frame = NULL;
	redraw(e);
}


GtkWidget *gtk_slide_view_new(Narrative *n, Slide *slide)
{
	GtkSlideView *sv;
	GtkTargetEntry targets[1];
	GError *err;

	sv = g_object_new(GTK_TYPE_SLIDE_VIEW, NULL);

	sv->n = n;
	sv->slide = slide;
	sv->w = 100;
	sv->h = 100;
	sv->border_offs_x = 0;
	sv->border_offs_y = 0;
	sv->min_border = 0.0;
	sv->h_scroll_pos = 0;
	sv->v_scroll_pos = 0;
	sv->view_scale = 1.0;

	err = NULL;
	sv->bg_pixbuf = gdk_pixbuf_new_from_resource("/uk/me/bitwiz/Colloquium/sky.png",
	                                                   &err);
	if ( sv->bg_pixbuf == NULL ) {
		fprintf(stderr, _("Failed to load background: %s\n"),
		        err->message);
	}

	gtk_widget_set_size_request(GTK_WIDGET(sv),
	                            sv->w, sv->h);

	g_signal_connect(G_OBJECT(sv), "destroy",
	                 G_CALLBACK(destroy_sig), sv);
	g_signal_connect(G_OBJECT(sv), "realize",
	                 G_CALLBACK(realise_sig), sv);
	g_signal_connect(G_OBJECT(sv), "button-press-event",
	                 G_CALLBACK(button_press_sig), sv);
	g_signal_connect(G_OBJECT(sv), "button-release-event",
	                 G_CALLBACK(button_release_sig), sv);
	g_signal_connect(G_OBJECT(sv), "motion-notify-event",
	                 G_CALLBACK(motion_sig), sv);
	g_signal_connect(G_OBJECT(sv), "configure-event",
	                 G_CALLBACK(resize_sig), sv);

	/* Drag and drop */
	targets[0].target = "text/uri-list";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(GTK_WIDGET(sv), 0, targets, 1,
	                  GDK_ACTION_PRIVATE);
	g_signal_connect(sv, "drag-data-received",
	                 G_CALLBACK(dnd_receive), sv);
	g_signal_connect(sv, "drag-motion",
	                 G_CALLBACK(dnd_motion), sv);
	g_signal_connect(sv, "drag-drop",
	                 G_CALLBACK(dnd_drop), sv);
	g_signal_connect(sv, "drag-leave",
	                 G_CALLBACK(dnd_leave), sv);

	gtk_widget_set_can_focus(GTK_WIDGET(sv), TRUE);
	gtk_widget_add_events(GTK_WIDGET(sv),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
	                       | GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(sv), "draw",
			 G_CALLBACK(draw_sig), sv);

	gtk_widget_grab_focus(GTK_WIDGET(sv));

	gtk_widget_show(GTK_WIDGET(sv));

	return GTK_WIDGET(sv);
}
