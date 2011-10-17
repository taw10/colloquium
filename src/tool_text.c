/*
 * tool_text.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>

#include "presentation.h"
#include "objects.h"
#include "mainwindow.h"
#include "slide_render.h"


enum text_drag_reason
{
	TEXT_DRAG_REASON_NONE,
	TEXT_DRAG_REASON_RESIZE,
};


struct text_toolinfo
{
	struct toolinfo        base;
	PangoContext          *pc;
	enum text_drag_reason  drag_reason;
	double                 box_x;
	double                 box_y;
	double                 box_width;
	double                 box_height;
	double                 drag_offset_x;
	double                 drag_offset_y;
};


struct text_object
{
	struct object        base;

	char                 *text;
	size_t                text_len;
	int                   insertion_point;
	int                   insertion_trail;
	PangoLayout          *layout;
	PangoFontDescription *fontdesc;
	double                offs_x;
	double                offs_y;
	int                   furniture;
};


static void calculate_size_from_style(struct text_object *o,
                                      double *peright, double *pebottom,
                                      double *pmw, double *pmh)
{
	double max_width, max_height;
	double ebottom, eright, mw, mh;

	eright = o->base.parent->parent->slide_width
	                      - o->base.style->margin_right;
	ebottom = o->base.parent->parent->slide_height
	                      - o->base.style->margin_bottom;
	mw = o->base.parent->parent->slide_width;
	mh = o->base.parent->parent->slide_height;

	*peright = eright;  *pebottom = ebottom;
	*pmw = mw;  *pmh = mh;

	max_width = mw - o->base.style->margin_left
		                      - o->base.style->margin_right;


	/* Use the provided maximum width if it exists and is smaller */
	if ( o->base.style->use_max_width
	     && (o->base.style->max_width < max_width) )
	{
		max_width = o->base.style->max_width;
	}

	max_height = mh - o->base.style->margin_top
		                           - o->base.style->margin_bottom;

	pango_layout_set_width(o->layout, max_width*PANGO_SCALE);
	pango_layout_set_height(o->layout, max_height*PANGO_SCALE);
	pango_layout_set_wrap(o->layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(o->layout, PANGO_ELLIPSIZE_MIDDLE);

	switch ( o->base.style->halign ) {
	case J_LEFT :
		pango_layout_set_alignment(o->layout, PANGO_ALIGN_LEFT);
		break;
	case J_RIGHT :
		pango_layout_set_alignment(o->layout, PANGO_ALIGN_RIGHT);
		break;
	case J_CENTER :
		pango_layout_set_alignment(o->layout, PANGO_ALIGN_CENTER);
		break;
	}
}


static void calculate_position_from_style(struct text_object *o,
                                          double eright, double ebottom,
                                          double mw, double mh)
{
	double xo, yo;

	xo = o->base.bb_width;  yo = o->base.bb_height;

	switch ( o->base.style->halign ) {
	case J_LEFT :
		o->base.x = o->base.style->margin_left;
		break;
	case J_RIGHT :
		o->base.x = eright - xo;
		break;
	case J_CENTER :
		o->base.x = mw/2.0 - xo/2.0 + o->base.style->offset_x;
		break;
	}

	/* Correct if centering crashes into opposite margin */
	if ( o->base.style->halign == J_CENTER )
	{
		if ( o->base.x < o->base.style->margin_left ) {
			o->base.x = o->base.style->margin_left;
		}

		if ( o->base.x + xo > eright ) {
			o->base.x = eright - xo;
		}
	}

	switch ( o->base.style->valign ) {
	case V_TOP :
		o->base.y = o->base.style->margin_top;
		break;
	case V_BOTTOM :
		o->base.y = ebottom - yo;
		break;
	case V_CENTER :
		o->base.y = mh/2.0 - yo/2.0 + o->base.style->offset_y;
		break;
	}

	if ( o->base.style->valign == V_CENTER ) {

		if ( o->base.y < o->base.style->margin_top ) {
			o->base.y = o->base.style->margin_top;
		}

		if ( o->base.y+yo + yo > ebottom )
		{
			o->base.y = ebottom - yo;
		}
	}
}


static void update_text(struct text_object *o)
{
	PangoRectangle logical;
	double eright = 0.0;
	double ebottom = 0.0;
	double mw = 0.0;
	double mh = 0.0;
	int furniture = 0;

	furniture = o->base.style != o->base.parent->parent->ss->styles[0];

	pango_layout_set_text(o->layout, o->text, -1);
	o->fontdesc = pango_font_description_from_string(o->base.style->font);
	pango_layout_set_font_description(o->layout, o->fontdesc);

	if ( o->furniture ) {

		calculate_size_from_style(o, &eright, &ebottom, &mw, &mh);

		pango_layout_get_extents(o->layout, NULL, &logical);

		o->base.bb_width = logical.width / PANGO_SCALE;
		o->base.bb_height = logical.height / PANGO_SCALE;
		o->offs_x = logical.x / PANGO_SCALE;
		o->offs_y = logical.y / PANGO_SCALE;

	} else {

		pango_layout_set_width(o->layout,
		                       o->base.bb_width*PANGO_SCALE);
		pango_layout_set_height(o->layout,
		                        o->base.bb_height*PANGO_SCALE);
		pango_layout_set_wrap(o->layout, PANGO_WRAP_WORD_CHAR);
		pango_layout_set_ellipsize(o->layout, PANGO_ELLIPSIZE_MIDDLE);

	}

	if ( o->furniture ) {
		calculate_position_from_style(o, eright, ebottom, mw, mh);
	}

	redraw_slide(((struct object *)o)->parent);
}


void insert_text(struct object *op, char *t)
{
	struct text_object *o = (struct text_object *)op;
	char *tmp;
	size_t tlen, olen, offs;
	int i;

	assert(o->base.type == TEXT);
	tlen = strlen(t);
	olen = strlen(o->text);

	if ( tlen + olen + 1 > o->text_len ) {

		char *try;

		try = realloc(o->text, o->text_len + tlen + 64);
		if ( try == NULL ) return;  /* Failed to insert */
		o->text = try;
		o->text_len += 64;
		o->text_len += tlen;

	}

	tmp = malloc(o->text_len);
	if ( tmp == NULL ) return;

	offs = o->insertion_point + o->insertion_trail;

	for ( i=0; i<offs; i++ ) {
		tmp[i] = o->text[i];
	}
	for ( i=0; i<tlen; i++ ) {
		tmp[i+offs] = t[i];
	}
	for ( i=0; i<olen-o->insertion_point; i++ ) {
		tmp[i+offs+tlen] = o->text[i+offs];
	}
	tmp[olen+tlen] = '\0';
	memcpy(o->text, tmp, o->text_len);
	free(tmp);

	update_text(o);
	o->insertion_point += tlen;
	o->base.empty = 0;
}


void move_cursor(struct object *op, int dir)
{
	struct text_object *o = (struct text_object *)op;
	int new_idx, new_trail;

	pango_layout_move_cursor_visually(o->layout, TRUE, o->insertion_point,
	                           o->insertion_trail,
	                           dir, &new_idx, &new_trail);

	if ( (new_idx >= 0) && (new_idx < G_MAXINT) ) {
		o->insertion_point = new_idx;
		o->insertion_trail = new_trail;
	}
}


void move_cursor_left(struct object *op)
{
	move_cursor(op, -1);
	redraw_overlay(op->parent->parent);
}


void move_cursor_right(struct object *op)
{
	move_cursor(op, +1);
	redraw_overlay(op->parent->parent);
}


void handle_text_backspace(struct object *op)
{
	int old_idx, new_idx;
	struct text_object *o = (struct text_object *)op;

	assert(o->base.type == TEXT);

	if ( o->insertion_point == 0 ) return;  /* Nothing to delete */

	old_idx = o->insertion_point + o->insertion_trail;
	move_cursor_left(op);
	new_idx = o->insertion_point + o->insertion_trail;

	memmove(o->text+new_idx, o->text+old_idx,
	        o->text_len-new_idx);

	if ( strlen(o->text) == 0 ) o->base.empty = 1;

	update_text(o);
}


static void render_text_object(cairo_t *cr, struct object *op)
{
	struct text_object *o = (struct text_object *)op;
	GdkColor col;

	cairo_move_to(cr, o->base.x - o->offs_x, o->base.y - o->offs_y);
	gdk_color_parse(o->base.style->colour, &col);
	gdk_cairo_set_source_color(cr, &col);  /* FIXME: Honour alpha as well */
	pango_cairo_update_layout(cr, o->layout);
	pango_cairo_show_layout(cr, o->layout);
}


static void draw_caret(cairo_t *cr, struct text_object *o)
{
	double xposd, yposd, cx;
	double clow, chigh;
	PangoRectangle pos;
	const double t = 1.8;

	assert(o->base.type == TEXT);

	pango_layout_get_cursor_pos(o->layout,
	                            o->insertion_point+o->insertion_trail,
	                            &pos, NULL);

	xposd = pos.x/PANGO_SCALE;
	cx = o->base.x - o->offs_x + xposd;
	yposd = pos.y/PANGO_SCALE;
	clow = o->base.y - o->offs_y + yposd;
	chigh = clow + (pos.height/PANGO_SCALE);

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


static void update_text_object(struct object *op)
{
	struct text_object *o = (struct text_object *)op;
	update_text(o);
}


static void delete_text_object(struct object *op)
{
	struct text_object *o = (struct text_object *)op;

	if ( o->layout != NULL ) g_object_unref(o->layout);
	if ( o->fontdesc != NULL ) pango_font_description_free(o->fontdesc);
}


static struct object *add_text_object(struct slide *s, double x, double y,
                                      struct style *sty,
                                      struct text_toolinfo *ti)
{
	struct text_object *new;

	new = calloc(1, sizeof(*new));
	if ( new == NULL ) return NULL;

	/* Base properties */
	new->base.x = x;  new->base.y = y;
	new->base.bb_width = 10.0;
	new->base.bb_height = 40.0;
	new->base.type = TEXT;
	new->base.empty = 1;
	new->base.parent = NULL;
	new->base.style = sty;

	/* Text-specific stuff */
	new->text = malloc(1);
	new->text[0] = '\0';
	new->text_len = 1;
	new->insertion_point = 0;
	new->insertion_trail = 0;
	new->layout = pango_layout_new(ti->pc);
	new->fontdesc = NULL;

	/* Methods for this object */
	new->base.render_object = render_text_object;
	new->base.delete_object = delete_text_object;
	new->base.update_object = update_text_object;

	if ( add_object_to_slide(s, (struct object *)new) ) {
		delete_object((struct object *)new);
		return NULL;
	}

	redraw_slide(((struct object *)new)->parent);

	return (struct object *)new;
}


static void click_select(struct presentation *p, struct toolinfo *tip,
                         double x, double y, GdkEventButton *event,
                         enum drag_status *drag_status,
	                 enum drag_reason *drag_reason)
{
	int xp, yp;
	double xo, yo;
	gboolean v;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;
	struct text_object *o = (struct text_object *)p->editing_object;
	int idx, trail;

	assert(o->base.type == TEXT);

	xo = x - o->base.x;  yo = y - o->base.y;

	/* Within the resizing region? */
	if ( (xo > o->base.bb_width - 20.0) && (yo > o->base.bb_height - 20.0)
	  && (!o->furniture) )
	{
		double cx, cy;

		ti->drag_reason = TEXT_DRAG_REASON_RESIZE;

		/* Initial size of rubber band box */
		ti->box_x = o->base.x;  ti->box_y = o->base.y;
		ti->box_width = o->base.bb_width;
		ti->box_height = o->base.bb_height;

		/* Coordinates of the bottom right corner */
		cx = o->base.x + o->base.bb_width;
		cy = o->base.y + o->base.bb_height;

		ti->drag_offset_x = x - cx;
		ti->drag_offset_y = y - cy;

		/* Tell the MCP what we did, and return */
		*drag_status = DRAG_STATUS_DRAGGING;
		*drag_reason = DRAG_REASON_TOOL;
		return;
	}

	xp = (xo + o->offs_x)*PANGO_SCALE;
	yp = (yo + o->offs_y)*PANGO_SCALE;

	v = pango_layout_xy_to_index(o->layout, xp, yp, &idx, &trail);

	o->insertion_point = idx;
	o->insertion_trail = trail;
}


static void drag(struct toolinfo *tip, struct presentation *p,
                 struct object *o, double x, double y)
{
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	ti->box_width = x - ti->drag_offset_x - ti->box_x;
	ti->box_height = y - ti->drag_offset_y - ti->box_y;
	if ( ti->box_width < 20.0 ) ti->box_width = 20.0;
	if ( ti->box_height < 20.0 ) ti->box_height = 20.0;

	redraw_overlay(p);
}


static void end_drag(struct toolinfo *tip, struct presentation *p,
                     struct object *o, double x, double y)
{
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	ti->box_width = x - ti->drag_offset_x - ti->box_x;
	ti->box_height = y - ti->drag_offset_y - ti->box_y;
	if ( ti->box_width < 20.0 ) ti->box_width = 20.0;
	if ( ti->box_height < 20.0 ) ti->box_height = 20.0;

	o->bb_width = ti->box_width;
	o->bb_height = ti->box_height;
	update_text((struct text_object *)o);

	ti->drag_reason = TEXT_DRAG_REASON_NONE;
}


static void create_default(struct presentation *p, struct style *sty,
                           struct toolinfo *tip)
{
	struct object *n;
	struct text_object *o;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	n = add_text_object(p->cur_edit_slide, 0.0, 0.0, sty, ti);
	o = (struct text_object *)n;
	o->furniture = 1;
	update_text(o);
	p->editing_object = n;
}


static void create_region(struct toolinfo *tip, struct presentation *p,
                          double x1, double y1, double x2, double y2)
{
	struct object *n;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;
	struct text_object *o;

	n = add_text_object(p->cur_edit_slide, 0.0, 0.0, p->ss->styles[0], ti);
	n->x = x1<x2 ? x1 : x2;
	n->y = y1<y2 ? y1 : y2;
	n->bb_width = fabs(x1-x2);
	n->bb_height = fabs(y1-y2);

	o = (struct text_object *)n;
	o->furniture = 0;

	update_text(o);
	p->editing_object = n;
}


static void select_object(struct object *o, struct toolinfo *tip)
{
	/* Do nothing */
}


static int deselect_object(struct object *o, struct toolinfo *tip)
{
	if ( (o != NULL) && o->empty ) {
		delete_object(o);
		return 1;
	}

	return 0;
}


static void draw_overlay(struct toolinfo *tip, cairo_t *cr, struct object *n)
{
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;
	struct text_object *o = (struct text_object *)n;

	if ( n != NULL ) {

		draw_editing_box(cr, n->x, n->y, n->bb_width, n->bb_height);

		if ( !o->furniture ) {

			/* Draw resize handle */
			cairo_new_path(cr);
			cairo_rectangle(cr, n->x+n->bb_width-20.0,
				            n->y+n->bb_height-20.0, 20.0, 20.0);
			cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
			cairo_fill(cr);

		}

		draw_caret(cr, o);
	}

	if ( ti->drag_reason == TEXT_DRAG_REASON_RESIZE ) {

		/* FIXME: Use common draw_rubberband_box() routine */
		cairo_new_path(cr);
		cairo_rectangle(cr, ti->box_x, ti->box_y,
		                    ti->box_width, ti->box_height);
		cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
		cairo_set_line_width(cr, 0.5);
		cairo_stroke(cr);

	}

}


static void key_pressed(struct object *o, guint keyval, struct toolinfo *tip)
{
	if ( o == NULL ) return;

	switch ( keyval ) {

	case GDK_KEY_BackSpace :
		handle_text_backspace(o);
		break;

	case GDK_KEY_Left :
		move_cursor_left(o);
		break;

	case GDK_KEY_Right :
		move_cursor_right(o);
		break;

	}
}


static void im_commit(struct object *o, gchar *str, struct toolinfo *tip)
{
	insert_text(o, str);
}


static int valid_object(struct object *o)
{
	if ( o->type == TEXT ) return 1;
	return 0;
}


struct toolinfo *initialise_text_tool(GtkWidget *w)
{
	struct text_toolinfo *ti;

	ti = malloc(sizeof(*ti));

	ti->pc = gtk_widget_get_pango_context(w);

	ti->base.click_select = click_select;
	ti->base.create_default = create_default;
	ti->base.select = select_object;
	ti->base.deselect = deselect_object;
	ti->base.drag = drag;
	ti->base.end_drag = end_drag;
	ti->base.create_region = create_region;
	ti->base.draw_editing_overlay = draw_overlay;
	ti->base.key_pressed = key_pressed;
	ti->base.im_commit = im_commit;
	ti->base.valid_object = valid_object;

	return (struct toolinfo *)ti;
}
