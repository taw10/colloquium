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

#include "presentation.h"
#include "objects.h"
#include "mainwindow.h"
#include "slide_render.h"


struct text_toolinfo
{
	struct toolinfo  base;
	PangoContext    *pc;
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

	calculate_size_from_style(o, &eright, &ebottom, &mw, &mh);

	pango_layout_get_extents(o->layout, NULL, &logical);
	o->base.bb_width = logical.width / PANGO_SCALE;
	o->base.bb_height = logical.height / PANGO_SCALE;
	o->offs_x = logical.x / PANGO_SCALE;
	o->offs_y = logical.y / PANGO_SCALE;

	if ( furniture ) {
		calculate_position_from_style(o, eright, ebottom, mw, mh);
	}
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
	o->base.parent->object_seq++;
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
}


void move_cursor_right(struct object *op)
{
	move_cursor(op, +1);
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
	o->base.parent->object_seq++;
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


static void draw_caret(cairo_t *cr, struct object *op)
{
	double xposd, yposd, cx;
	double clow, chigh;
	PangoRectangle pos;
	const double t = 1.8;
	struct text_object *o = (struct text_object *)op;

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

	if ( add_object_to_slide(s, (struct object *)new) ) {
		delete_object((struct object *)new);
		return NULL;
	}
	s->object_seq++;

	return (struct object *)new;
}


static void click_create(struct presentation *p, struct toolinfo *tip,
                         double x, double y)
{
	struct object *n;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	/* FIXME: Insert ESP here and possibly select a different style */
	n = add_text_object(p->view_slide, x, y, p->ss->styles[0], ti);
	p->editing_object = n;
}


static void click_select(struct presentation *p, struct toolinfo *tip,
                         double x, double y)
{
	int xp, yp;
	gboolean v;
	struct text_object *o = (struct text_object *)p->editing_object;
	int idx, trail;

	assert(o->base.type == TEXT);

	xp = (x - o->base.x + o->offs_x)*PANGO_SCALE;
	yp = (y - o->base.y + o->offs_y)*PANGO_SCALE;

	v = pango_layout_xy_to_index(o->layout, xp, yp, &idx, &trail);

	o->insertion_point = idx;
	o->insertion_trail = trail;
}


static void drag_object(struct toolinfo *tip, struct presentation *p,
                        struct object *o, double x, double y)
{
	/* Do nothing */
}


static void create_default(struct presentation *p, struct style *sty,
                           struct toolinfo *tip)
{
	struct object *n;
	struct text_toolinfo *ti = (struct text_toolinfo *)tip;

	n = add_text_object(p->view_slide, 0.0, 0.0, sty, ti);
	p->editing_object = n;
}


static void select_object(struct object *o,struct toolinfo *tip)
{
	/* Do nothing */
}


static int deselect_object(struct object *o,struct toolinfo *tip)
{
	if ( (o != NULL) && o->empty ) {
		delete_object(o);
		return 1;
	}

	return 0;
}


static void draw_overlay(cairo_t *cr, struct object *o)
{
	draw_editing_box(cr, o->x, o->y,
	                     o->bb_width, o->bb_height);
	draw_caret(cr, o);
}


struct toolinfo *initialise_text_tool(GtkWidget *w)
{
	struct text_toolinfo *ti;

	ti = malloc(sizeof(*ti));

	ti->pc = gtk_widget_get_pango_context(w);

	ti->base.click_create = click_create;
	ti->base.click_select = click_select;
	ti->base.create_default = create_default;
	ti->base.select = select_object;
	ti->base.deselect = deselect_object;
	ti->base.drag_object = drag_object;
	ti->base.draw_editing_overlay = draw_overlay;

	return (struct toolinfo *)ti;
}
