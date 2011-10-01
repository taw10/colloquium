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


void insert_text(struct object *o, char *t)
{
	char *tmp;
	size_t tlen, olen;
	int i;

	assert(o->type == TEXT);
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

	for ( i=0; i<o->insertion_point; i++ ) {
		tmp[i] = o->text[i];
	}
	for ( i=0; i<tlen; i++ ) {
		tmp[i+o->insertion_point] = t[i];
	}
	for ( i=0; i<olen-o->insertion_point; i++ ) {
		tmp[i+o->insertion_point+tlen] = o->text[i+o->insertion_point];
	}
	tmp[olen+tlen] = '\0';
	memcpy(o->text, tmp, o->text_len);
	free(tmp);

	o->insertion_point += tlen;
	o->parent->object_seq++;
	o->empty = 0;
}


void set_text_style(struct object *o, struct style *sty)
{
	assert(o->type == TEXT);
	o->style = sty;
	o->parent->object_seq++;
}


static int find_prev_index(const char *t, int p)
{
	int i, nback;

	if ( p == 0 ) return 0;

	if ( !(t[p-1] & 0x80) ) {
		nback = 1;
	} else {
		nback = 0;
		for ( i=1; i<=6; i++ ) {
			if ( p-i == 0 ) return 0;
			if ( !(t[p-i] & 0xC0) ) nback++;
		}
	}

	return p - nback;
}


static int find_next_index(const char *t, int p)
{
	int i, nfor;

	if ( t[p] == '\0' ) return p;

	if ( !(t[p+1] & 0x80) ) {
		nfor = 1;
	} else {
		nfor = 0;
		for ( i=1; i<=6; i++ ) {
			if ( t[p+i] == '\0' ) return p+i;
			if ( !(t[p+i] & 0xC0) ) nfor++;
		}
	}

	return p + nfor;
}


void handle_text_backspace(struct object *o)
{
	int prev_index;

	assert(o->type == TEXT);

	if ( o->insertion_point == 0 ) return;  /* Nothing to delete */

	prev_index = find_prev_index(o->text, o->insertion_point);

	memmove(o->text+prev_index, o->text+o->insertion_point,
	        o->text_len-o->insertion_point);

	o->insertion_point = prev_index;

	if ( strlen(o->text) == 0 ) o->empty = 1;

	o->parent->object_seq++;
}


void move_cursor_left(struct object *o)
{
	o->insertion_point = find_prev_index(o->text, o->insertion_point);
}


void move_cursor_right(struct object *o)
{
	o->insertion_point = find_next_index(o->text, o->insertion_point);
}


void position_caret(struct object *o, double x, double y)
{
	int idx, trail;
	int xp, yp;
	gboolean v;

	assert(o->type == TEXT);

	xp = (x - o->x)*PANGO_SCALE;
	yp = (y - o->y)*PANGO_SCALE;

	v = pango_layout_xy_to_index(o->layout, xp, yp, &idx, &trail);

	o->insertion_point = idx+trail;
}


static void calculate_size_from_style(struct object *o,
                                      double *peright, double *pebottom,
                                      double *pmw, double *pmh)
{
	double max_width, max_height;
	double ebottom, eright, mw, mh;

	eright = o->parent->parent->slide_width - o->style->margin_right;
	ebottom = o->parent->parent->slide_height - o->style->margin_bottom;
	mw = o->parent->parent->slide_width;
	mh = o->parent->parent->slide_height;

	*peright = eright;  *pebottom = ebottom;
	*pmw = mw;  *pmh = mh;

	max_width = mw - o->style->margin_left - o->style->margin_right;

	/* Use the provided maximum width if it exists and is smaller */
	if ( o->style->use_max_width && (o->style->max_width < max_width) )
	{
		max_width = o->style->max_width;
	}

	max_height = mh - o->style->margin_top - o->style->margin_bottom;

	pango_layout_set_width(o->layout, max_width*PANGO_SCALE);
	pango_layout_set_height(o->layout, max_height*PANGO_SCALE);
	pango_layout_set_wrap(o->layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(o->layout, PANGO_ELLIPSIZE_MIDDLE);

	switch ( o->style->halign ) {
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


static void calculate_position_from_style(struct object *o,
                                          double eright, double ebottom,
                                          double mw, double mh,
                                          double *pxo, double *pyo)
{
	double xo, yo;
	PangoRectangle ink, logical;

	pango_layout_get_extents(o->layout, &ink, &logical);
	xo = ink.x/PANGO_SCALE;  yo = logical.y/PANGO_SCALE;

	switch ( o->style->halign ) {
	case J_LEFT :
		o->x = -xo + o->style->margin_left;
		break;
	case J_RIGHT :
		o->x = -xo + eright - o->bb_width;
		break;
	case J_CENTER :
		o->x = mw/2.0 - o->bb_width/2.0 - xo + o->style->offset_x;
		break;
	}

	if ( o->style->halign == J_CENTER )
	{
		if ( o->x+xo < o->style->margin_left ) {
			o->x = o->style->margin_left - xo;
		}

		if ( o->x+xo + o->bb_width > mw-o->style->margin_right ) {
			o->x = mw-o->style->margin_right - xo - o->bb_width;
		}
	}

	switch ( o->style->valign ) {
	case V_TOP :
		o->y = o->style->margin_top;
		break;
	case V_BOTTOM :
		o->y = ebottom - o->bb_height;
		break;
	case V_CENTER :
		o->y = mh/2.0 - o->bb_height/2.0 + yo - o->style->offset_y;
		break;
	}

	if ( o->style->valign == V_CENTER ) {

		if ( o->y < o->style->margin_top ) {
			o->y = o->style->margin_top;
		}

		if ( o->y+yo + o->bb_height > mh - o->style->margin_bottom ) {
			o->y = mh-o->style->margin_bottom - yo - o->bb_height;
		}
	}

	*pxo = xo;  *pyo = yo;
}


static void render_text_object(cairo_t *cr, struct object *o)
{
	PangoRectangle ink, logical;
	double eright = 0.0;
	double ebottom = 0.0;
	double mw = 0.0;
	double mh = 0.0;
	double xo, yo;
	int furniture = 0;
	GdkColor col;

	furniture = o->style != o->parent->parent->ss->styles[0];

	o->layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(o->layout, o->text, -1);
	o->fontdesc = pango_font_description_from_string(o->style->font);
	pango_layout_set_font_description(o->layout, o->fontdesc);

	if ( furniture ) {
		calculate_size_from_style(o, &eright, &ebottom, &mw, &mh);
	} else {
		pango_layout_set_alignment(o->layout, PANGO_ALIGN_LEFT);
	}

	pango_cairo_update_layout(cr, o->layout);
	pango_layout_get_extents(o->layout, &ink, &logical);
	o->bb_width = ink.width / PANGO_SCALE;
	o->bb_height = logical.height/PANGO_SCALE;

	if ( furniture ) {
		calculate_position_from_style(o, eright, ebottom,
		                              mw, mh, &xo, &yo);
	}

	cairo_move_to(cr, o->x, o->y);
	gdk_color_parse(o->style->colour, &col);
	gdk_cairo_set_source_color(cr, &col);  /* FIXME: Honour alpha as well */
	pango_cairo_show_layout(cr, o->layout);

	if ( furniture ) {
		o->x += xo;
		o->y += yo;
	}
}


static void draw_caret(cairo_t *cr, struct object *o)
{
	int line, xpos;
	double xposd;

	assert(o->type == TEXT);

	pango_layout_index_to_line_x(o->layout, o->insertion_point,
	                             0, &line, &xpos);

	xposd = xpos/PANGO_SCALE;

	cairo_move_to(cr, o->x+xposd, o->y);
	cairo_line_to(cr, o->x+xposd, o->y+o->bb_height);
	cairo_set_source_rgb(cr, 1.0, 0.5, 0.0);
	cairo_set_line_width(cr, 2.0);
	cairo_stroke(cr);
}


struct object *add_text_object(struct slide *s, double x, double y,
                               struct style *sty)
{
	struct object *new;

	new = new_object(TEXT, sty);

	new->x = x;  new->y = y;
	new->bb_width = 10.0;
	new->bb_height = 40.0;
	new->text = malloc(1);
	new->text[0] = '\0';
	new->text_len = 1;
	new->insertion_point = 0;
	new->layout = NULL;
	new->fontdesc = NULL;

	/* Methods for this object */
	new->render_object = render_text_object;
	new->draw_editing_overlay = draw_caret;

	if ( add_object_to_slide(s, new) ) {
		free_object(new);
		return NULL;
	}
	s->object_seq++;

	return new;
}
