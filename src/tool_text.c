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
#include "loadsave.h"


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
	enum corner            drag_corner;
	double                 box_x;
	double                 box_y;
	double                 box_width;
	double                 box_height;
	double                 drag_initial_x;
	double                 drag_initial_y;
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

	PangoContext        **pc;
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

	if ( o->layout == NULL ) {
		if ( *o->pc != NULL ) {
			o->layout = pango_layout_new(*o->pc);
		} else {
			/* Can't render yet */
			return;
		}
	}

	o->furniture = o->base.style != o->base.parent->parent->ss->styles[0];

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
}


void insert_text(struct object *op, char *t)
{
	struct text_object *o = (struct text_object *)op;
	char *tmp;
	size_t tlen, olen, offs;
	int i;

	assert(o->base.type == OBJ_TEXT);
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
	redraw_slide(op->parent);
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

	assert(o->base.type == OBJ_TEXT);

	if ( o->insertion_point == 0 ) return;  /* Nothing to delete */

	old_idx = o->insertion_point + o->insertion_trail;
	move_cursor_left(op);
	new_idx = o->insertion_point + o->insertion_trail;

	memmove(o->text+new_idx, o->text+old_idx,
	        o->text_len-new_idx);

	if ( strlen(o->text) == 0 ) o->base.empty = 1;

	update_text(o);
	redraw_slide(op->parent);
}


static void render_text_object(cairo_t *cr, struct object *op)
{
	struct text_object *o = (struct text_object *)op;
	GdkColor col;

	if ( o->layout == NULL ) {
		return;
	}

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

	assert(o->base.type == OBJ_TEXT);

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


static void serialize(struct object *op, struct serializer *ser)
{
	struct text_object *o = (struct text_object *)op;

	serialize_s(ser, "style", op->style->name);
	if ( op->style == op->parent->parent->ss->styles[0] ) {
		serialize_f(ser, "x", op->x);
		serialize_f(ser, "y", op->y);
		serialize_f(ser, "w", op->bb_width);
		serialize_f(ser, "h", op->bb_height);
	}

	serialize_s(ser, "text", o->text);
}


static struct object *new_text_object(double x, double y, struct style *sty,
                                      struct text_toolinfo *ti)
{
	struct text_object *new;

	new = calloc(1, sizeof(*new));
	if ( new == NULL ) return NULL;

	/* Base properties */
	new->base.x = x;  new->base.y = y;
	new->base.bb_width = 10.0;
	new->base.bb_height = 40.0;
	new->base.type = OBJ_TEXT;
	new->base.empty = 1;
	new->base.parent = NULL;
	new->base.style = sty;

	/* Text-specific stuff */
	new->text = malloc(1);
	new->text[0] = '\0';
	new->text_len = 1;
	new->insertion_point = 0;
	new->insertion_trail = 0;
	if ( ti->pc != NULL ) {
		new->layout = pango_layout_new(ti->pc);
		new->pc = NULL;
	} else {
		new->layout = NULL;
		new->pc = &ti->pc;
	}
	new->fontdesc = NULL;

	/* Methods for this object */
	new->base.render_object = render_text_object;
	new->base.delete_object = delete_text_object;
	new->base.update_object = update_text_object;
	new->base.serialize = serialize;

	return (struct object *)new;
}


static struct object *add_text_object(struct slide *s, double x, double y,
                                      struct style *sty,
                                      struct text_toolinfo *ti)
{
	struct object *o;

	o = new_text_object(x, y, sty, ti);

	if ( add_object_to_slide(s, o) ) {
		delete_object(o);
		return NULL;
	}

	redraw_slide(o->parent);

	return o;
}


static void calculate_box_size(struct object *o, double x, double y,
                               struct text_toolinfo *ti)
{
	double ddx, ddy;

	ddx = x - ti->drag_initial_x;
	ddy = y - ti->drag_initial_y;

	switch ( ti->drag_corner ) {

		case CORNER_BR :
		ti->box_x = o->x;
		ti->box_y = o->y;
		ti->box_width = o->bb_width + ddx;
		ti->box_height = o->bb_height + ddy;
		break;

		case CORNER_BL :
		ti->box_x = o->x + ddx;
		ti->box_y = o->y;
		ti->box_width = o->bb_width - ddx;
		ti->box_height = o->bb_height + ddy;
		break;

		case CORNER_TL :
		ti->box_x = o->x + ddx;
		ti->box_y = o->y + ddy;
		ti->box_width = o->bb_width - ddx;
		ti->box_height = o->bb_height - ddy;
		break;

		case CORNER_TR :
		ti->box_x = o->x;
		ti->box_y = o->y + ddy;
		ti->box_width = o->bb_width + ddx;
		ti->box_height = o->bb_height - ddy;
		break;

		case CORNER_NONE :
		break;

	}

	if ( ti->box_width < 20.0 ) ti->box_width = 20.0;
	if ( ti->box_height < 20.0 ) ti->box_height = 20.0;
}


static void click_select(struct presentation *p, struct toolinfo *tip,
                         double x, double y, GdkEventButton *event,
                         enum drag_status *drag_status,
	                 enum drag_reason *drag_reason)
{
	int xp, yp;
	double xo, yo;
	gboolean v;
	enum corner c;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;
	struct text_object *o = (struct text_object *)p->editing_object;
	int idx, trail;

	assert(o->base.type == OBJ_TEXT);

	xo = x - o->base.x;  yo = y - o->base.y;

	/* Within the resizing region? */
	c = which_corner(x, y, &o->base);
	if ( (c != CORNER_NONE) && !o->furniture )
	{
		ti->drag_reason = TEXT_DRAG_REASON_RESIZE;
		ti->drag_corner = c;

		ti->drag_initial_x = x;
		ti->drag_initial_y = y;

		calculate_box_size((struct object *)o, x, y, ti);

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

	if ( ti->drag_reason == TEXT_DRAG_REASON_RESIZE ) {

		calculate_box_size(o, x, y, ti);
		redraw_overlay(p);

	}
}


static void end_drag(struct toolinfo *tip, struct presentation *p,
                     struct object *o, double x, double y)
{
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	calculate_box_size((struct object *)o, x, y, ti);

	o->x = ti->box_x;
	o->y = ti->box_y;
	o->bb_width = ti->box_width;
	o->bb_height = ti->box_height;
	update_text((struct text_object *)o);
	redraw_slide(o->parent);

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
	redraw_slide(((struct object *)o)->parent);
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
	redraw_slide(((struct object *)o)->parent);
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

			/* Draw resize handles */
			draw_resize_handle(cr, n->x, n->y+n->bb_height-20.0);
			draw_resize_handle(cr, n->x+n->bb_width-20.0, n->y);
			draw_resize_handle(cr, n->x, n->y);
			draw_resize_handle(cr, n->x+n->bb_width-20.0,
				           n->y+n->bb_height-20.0);

		}

		draw_caret(cr, o);
	}

	if ( ti->drag_reason == TEXT_DRAG_REASON_RESIZE ) {
		draw_rubberband_box(cr, ti->box_x, ti->box_y,
		                        ti->box_width, ti->box_height);
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
	if ( o->type == OBJ_TEXT ) return 1;
	return 0;
}


static void realise(struct toolinfo *tip, GtkWidget *w)
{
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;
	ti->pc = gtk_widget_get_pango_context(w);
}


static struct object *deserialize(struct presentation *p, struct ds_node *root,
                                  struct toolinfo *tip)
{
	struct object *o;
	struct text_object *to;
	char *style;
	char *text;
	struct style *sty;
	double x, y, w, h;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	if ( get_field_s(root, "style", &style) ) {
		fprintf(stderr, "Couldn't find style for object '%s'\n",
		        root->key);
		return NULL;
	}

	sty = find_style(p->ss, style);
	if ( sty == NULL ) {
		fprintf(stderr, "Style '%s' not found in style sheet.\n",
		        style);
		free(style);
		return NULL;
	}
	free(style);

	if ( sty == p->ss->styles[0] ) {

		if ( get_field_f(root, "x", &x) ) {
			fprintf(stderr,
			        "Couldn't find x position for object '%s'\n",
				root->key);
			return NULL;
		}
		if ( get_field_f(root, "y", &y) ) {
			fprintf(stderr,
			        "Couldn't find y position for object '%s'\n",
				root->key);
			return NULL;
		}
		if ( get_field_f(root, "w", &w) ) {
			fprintf(stderr,
			        "Couldn't find width for object '%s'\n",
				root->key);
			return NULL;
		}
		if ( get_field_f(root, "h", &h) ) {
			fprintf(stderr,
			        "Couldn't find height for object '%s'\n",
				root->key);
			return NULL;
		}

	} else {

		/* Furniture */
		x = 0.0;
		y = 0.0;
		w = 0.0;
		h = 0.0;

	}

	o = new_text_object(x, y, sty, ti);
	o->bb_width = w;
	o->bb_height = h;

	/* Apply the correct text */
	if ( get_field_s(root, "text", &text) ) {
		fprintf(stderr, "Couldn't find text for object '%s'\n",
		        root->key);
		return NULL;
	}
	to = (struct text_object *)o;
	free(to->text);
	to->text = text;
	to->text_len = strlen(text);
	o->empty = 0;

        return o;
}


struct toolinfo *initialise_text_tool(GtkWidget *w)
{
	struct text_toolinfo *ti;

	ti = malloc(sizeof(*ti));

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
	ti->base.realise = realise;
	ti->base.deserialize = deserialize;

	ti->pc = NULL;

	ti->drag_reason = DRAG_REASON_NONE;

	return (struct toolinfo *)ti;
}
