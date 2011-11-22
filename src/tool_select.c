/*
 * tool_sekect.c
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


enum select_drag_reason
{
	SELECT_DRAG_REASON_NONE,
	SELECT_DRAG_REASON_POTENTIAL_MOVE,
	SELECT_DRAG_REASON_MOVE,
};


struct select_toolinfo
{
	struct toolinfo base;
	double drag_offs_x;
	double drag_offs_y;
	double box_x;
	double box_y;
	double box_width;
	double box_height;
	enum select_drag_reason drag_reason;
};


static void click_select(struct presentation *p, struct toolinfo *tip,
                         double x, double y, GdkEventButton *event,
                         enum drag_status *drag_status,
	                 enum drag_reason *drag_reason)
{
	struct select_toolinfo *ti = (struct select_toolinfo *)tip;
	struct object *clicked = p->editing_object;

	ti->drag_offs_x = clicked->x - x;
	ti->drag_offs_y = clicked->y - y;

	*drag_status = DRAG_STATUS_COULD_DRAG;
	*drag_reason = DRAG_REASON_TOOL;
	ti->drag_reason = SELECT_DRAG_REASON_POTENTIAL_MOVE;
}


static void update_box(struct select_toolinfo *ti, double x, double y,
                       struct object *o)
{
	double nx, ny;
	double eright, ebottom;

	nx = x + ti->drag_offs_x;
	ny = y + ti->drag_offs_y;

	/* Enforce margins */
	eright = o->parent->parent->slide_width - o->style->margin_right;
	ebottom = o->parent->parent->slide_height - o->style->margin_bottom;
	if ( nx < o->style->margin_left ) nx = o->style->margin_left;
	if ( ny < o->style->margin_top ) ny = o->style->margin_top;
	if ( nx+o->bb_width > eright ) nx = eright - o->bb_width;
	if ( ny+o->bb_height > ebottom ) ny = ebottom - o->bb_height;

	ti->box_x = nx;
	ti->box_y = ny;
	ti->box_width = o->bb_width;
	ti->box_height = o->bb_height;
}


static void drag(struct toolinfo *tip, struct presentation *p,
                 struct object *o, double x, double y)
{
	struct select_toolinfo *ti = (struct select_toolinfo *)tip;

	if ( ti->drag_reason == SELECT_DRAG_REASON_POTENTIAL_MOVE ) {
		ti->drag_reason = SELECT_DRAG_REASON_MOVE;
	}

	if ( ti->drag_reason != SELECT_DRAG_REASON_MOVE ) return;

	update_box(ti, x, y, o);

	redraw_overlay(p);
}


static void end_drag(struct toolinfo *tip, struct presentation *p,
                     struct object *o, double x, double y)
{
	struct select_toolinfo *ti = (struct select_toolinfo *)tip;

	update_box(ti, x, y, o);
	o->x = ti->box_x;
	o->y = ti->box_y;
	ti->drag_reason = SELECT_DRAG_REASON_NONE;
	redraw_slide(o->parent);
}


static void create_region(struct toolinfo *tip, struct presentation *p,
                          double x1, double y1, double x2, double y2)
{
	/* FIXME: Select multiple objects */
}


static void select_object(struct object *o,struct toolinfo *tip)
{
	/* Do nothing */
}


static int deselect_object(struct object *o,struct toolinfo *tip)
{
	/* Do nothing */
	return 0;
}


static void draw_overlay(struct toolinfo *tip, cairo_t *cr, struct object *o)
{
	struct select_toolinfo *ti = (struct select_toolinfo *)tip;

	if ( o != NULL ) {
		draw_editing_box(cr, o->x, o->y, o->bb_width, o->bb_height);
	}

	if ( ti->drag_reason == SELECT_DRAG_REASON_MOVE ) {
		draw_rubberband_box(cr, ti->box_x, ti->box_y,
		                        ti->box_width, ti->box_height);
	}
}


static void key_pressed(struct object *o, guint keyval, struct toolinfo *tip)
{
	/* Do nothing */
}


static void im_commit(struct object *o, gchar *str, struct toolinfo *tip)
{
	/* Do nothing */
}


static int valid_object(struct object *o)
{
	return 1;
}


struct toolinfo *initialise_select_tool()
{
	struct select_toolinfo *ti;

	ti = malloc(sizeof(*ti));

	ti->drag_reason = SELECT_DRAG_REASON_NONE;

	ti->base.click_select = click_select;
	ti->base.create_default = NULL;
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
