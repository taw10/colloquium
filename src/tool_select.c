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


struct select_toolinfo
{
	struct toolinfo base;
	double drag_offs_x;
	double drag_offs_y;
};


static void click_create(struct presentation *p, struct toolinfo *tip,
                         double x, double y)
{
	/* Do absolutely nothing */
}


static void click_select(struct presentation *p, struct toolinfo *tip,
                         double x, double y)
{
	struct select_toolinfo *ti = (struct select_toolinfo *)tip;
	struct object *clicked = p->editing_object;

	ti->drag_offs_x = clicked->x - x;
	ti->drag_offs_y = clicked->y - y;
}


static void drag_object(struct toolinfo *tip, struct presentation *p,
                        struct object *o, double x, double y)
{
	struct select_toolinfo *ti = (struct select_toolinfo *)tip;
	double eright, ebottom;

	o->x = x + ti->drag_offs_x;
	o->y = y + ti->drag_offs_y;

	/* Enforce margins */
	eright = o->parent->parent->slide_width - o->style->margin_right;
	ebottom = o->parent->parent->slide_height - o->style->margin_bottom;
	if ( o->x < o->style->margin_left ) o->x = o->style->margin_left;
	if ( o->y < o->style->margin_top ) o->y = o->style->margin_top;
	if ( o->x+o->bb_width > eright ) o->x = eright - o->bb_width;
	if ( o->y+o->bb_height > ebottom ) o->y = ebottom - o->bb_height;

	o->update_object(o);
	p->view_slide->object_seq++;

	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);
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


static void draw_overlay(cairo_t *cr, struct object *o)
{
	draw_editing_box(cr, o->x, o->y, o->bb_width, o->bb_height);
}


static void key_pressed(struct object *o, guint keyval, struct toolinfo *tip)
{
	/* Do nothing */
}


static void im_commit(struct object *o, gchar *str, struct toolinfo *tip)
{
	/* Do nothing */
}


struct toolinfo *initialise_select_tool()
{
	struct select_toolinfo *ti;

	ti = malloc(sizeof(*ti));

	ti->base.click_create = click_create;
	ti->base.click_select = click_select;
	ti->base.create_default = NULL;
	ti->base.select = select_object;
	ti->base.deselect = deselect_object;
	ti->base.drag_object = drag_object;
	ti->base.draw_editing_overlay = draw_overlay;
	ti->base.key_pressed = key_pressed;
	ti->base.im_commit = im_commit;

	return (struct toolinfo *)ti;
}
