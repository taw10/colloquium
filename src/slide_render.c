/*
 * slide_render.c
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

#include <cairo.h>
#include <pango/pangocairo.h>
#include <assert.h>

#include "slide_render.h"
#include "presentation.h"
#include "objects.h"
#include "stylesheet.h"


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


void draw_caret(cairo_t *cr, struct object *o)
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


int render_slide(struct slide *s)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                  s->parent->slide_width,
	                                  s->parent->slide_height);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0,
	                s->parent->slide_width, s->parent->slide_height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	for ( i=0; i<s->num_objects; i++ ) {

		struct object *o = s->objects[i];

		switch ( o->type ) {
		case TEXT :
			render_text_object(cr, o);
			break;
		}

	}

	cairo_destroy(cr);

	if ( s->render_cache != NULL ) cairo_surface_destroy(s->render_cache);
	s->render_cache = surf;
	s->render_cache_seq = s->object_seq;

	return 0;
}


void check_redraw_slide(struct slide *s)
{
	/* Update necessary? */
	if ( s->object_seq <= s->render_cache_seq ) return;

	render_slide(s);
}
