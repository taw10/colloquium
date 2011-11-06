/*
 * tool_image.c
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


enum image_drag_reason
{
	IMAGE_DRAG_REASON_NONE,
	IMAGE_DRAG_REASON_RESIZE,
};


struct image_toolinfo
{
	struct toolinfo        base;
	enum image_drag_reason drag_reason;
	double                 box_x;
	double                 box_y;
	double                 box_width;
	double                 box_height;
	double                 drag_offset_x;
	double                 drag_offset_y;
};


struct image_object
{
	struct object        base;

	struct image        *image;
	GdkPixbuf           *scaled_pb;
	int                  scaled_w;
	int                  scaled_h;
};


static void update_image(struct image_object *o)
{
	struct image *i = o->image;
	int w, h;

	/* Fit the width and calculate the height */
	w = o->base.bb_width;
	h = ((double)i->height / i->width) * o->base.bb_width;
	if ( h > o->base.bb_height ) {
		h = o->base.bb_height;
		w = ((double)i->width / i->height) * o->base.bb_height;
	}

	if ( (o->scaled_w != w) || (o->scaled_h != h) ) {
		if ( o->scaled_pb != NULL ) gdk_pixbuf_unref(o->scaled_pb);
		o->scaled_pb = gdk_pixbuf_scale_simple(i->pb, w, h,
			                               GDK_INTERP_BILINEAR);
	} /* else the size didn't change */

	redraw_slide(((struct object *)o)->parent);
}


static void render_image_object(cairo_t *cr, struct object *op)
{
	struct image_object *o = (struct image_object *)op;

	cairo_new_path(cr);
	cairo_rectangle(cr, op->x, op->y, op->bb_width, op->bb_height);
	gdk_cairo_set_source_pixbuf(cr, o->scaled_pb, op->x, op->y);
	cairo_fill(cr);
}



static void update_image_object(struct object *op)
{
	struct image_object *o = (struct image_object *)op;
	update_image(o);
}


static void delete_image_object(struct object *op)
{
	struct image_object *o = (struct image_object *)op;
	if ( o->scaled_pb != NULL ) gdk_pixbuf_unref(o->scaled_pb);
	unref_image(o->image);
}


static void serialize(struct object *op, struct serializer *ser)
{
	struct image_object *o = (struct image_object *)op;

	serialize_f(ser, "x", op->x);
	serialize_f(ser, "y", op->y);
	serialize_f(ser, "w", op->bb_width);
	serialize_f(ser, "h", op->bb_height);
	serialize_s(ser, "filename", o->image->filename);
}


static struct image_object *new_image_object(double x, double y,
                                             double bb_width, double bb_height,
                                             char *filename, struct style *sty,
                                             struct image_store *is,
                                             struct image_toolinfo *ti)
{
	struct image_object *new;

	new = calloc(1, sizeof(*new));
	if ( new == NULL ) return NULL;

	/* Base properties */
	new->base.x = x;  new->base.y = y;
	new->base.bb_width = bb_width;
	new->base.bb_height = bb_height;
	new->base.type = OBJ_IMAGE;
	new->base.empty = 0;
	new->base.parent = NULL;
	new->base.style = sty;

	new->scaled_pb = NULL;
	new->image = get_image(is, filename);
	if ( new->image == NULL ) {
		free(new);
		printf("Failed to load or get image.\n");
		return NULL;
	}

	/* Methods for this object */
	new->base.render_object = render_image_object;
	new->base.delete_object = delete_image_object;
	new->base.update_object = update_image_object;
	new->base.serialize = serialize;

	return new;
}


struct object *add_image_object(struct slide *s, double x, double y,
                                double bb_width, double bb_height,
                                char *filename, struct style *sty,
                                struct image_store *is,
                                struct image_toolinfo *ti)
{
	struct image_object *new;

	new = new_image_object(x, y, bb_width, bb_height,
	                       filename, sty, is, ti);
	if ( new == NULL ) return NULL;

	new->base.parent = s;
	if ( add_object_to_slide(s, (struct object *)new) ) {
		free(new);
		return NULL;
	}

	return (struct object *)new;
}


static void click_select(struct presentation *p, struct toolinfo *tip,
                         double x, double y, GdkEventButton *event,
                         enum drag_status *drag_status,
	                 enum drag_reason *drag_reason)
{
	double xo, yo;
	struct image_toolinfo *ti = (struct image_toolinfo *)tip;
	struct image_object *o = (struct image_object *)p->editing_object;

	assert(o->base.type == OBJ_IMAGE);

	xo = x - o->base.x;  yo = y - o->base.y;

	/* Within the resizing region? */
	if ( (xo > o->base.bb_width - 20.0) && (yo > o->base.bb_height - 20.0) )
	{
		double cx, cy;

		ti->drag_reason = IMAGE_DRAG_REASON_RESIZE;

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
}


static void drag(struct toolinfo *tip, struct presentation *p,
                 struct object *o, double x, double y)
{
	struct image_toolinfo *ti = (struct image_toolinfo *)tip;

	ti->box_width = x - ti->drag_offset_x - ti->box_x;
	ti->box_height = y - ti->drag_offset_y - ti->box_y;
	if ( ti->box_width < 20.0 ) ti->box_width = 20.0;
	if ( ti->box_height < 20.0 ) ti->box_height = 20.0;

	redraw_overlay(p);
}


static void end_drag(struct toolinfo *tip, struct presentation *p,
                     struct object *o, double x, double y)
{
	struct image_toolinfo *ti = (struct image_toolinfo *)tip;

	ti->box_width = x - ti->drag_offset_x - ti->box_x;
	ti->box_height = y - ti->drag_offset_y - ti->box_y;
	if ( ti->box_width < 20.0 ) ti->box_width = 20.0;
	if ( ti->box_height < 20.0 ) ti->box_height = 20.0;

	o->bb_width = ti->box_width;
	o->bb_height = ti->box_height;
	update_image((struct image_object *)o);

	ti->drag_reason = IMAGE_DRAG_REASON_NONE;
}


static void create_region(struct toolinfo *tip, struct presentation *p,
                          double x1, double y1, double x2, double y2)
{
	//struct object *n;
	//struct image_toolinfo *ti = (struct image_toolinfo *)tip;
	//struct image_object *o;

	/* FIXME: Open an "Open file" dialogue box and use the result */
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
	struct image_toolinfo *ti = (struct image_toolinfo *)tip;
	//struct image_object *o = (struct image_object *)n;

	if ( n != NULL ) {

		draw_editing_box(cr, n->x, n->y, n->bb_width, n->bb_height);

		/* Draw resize handle */
		cairo_new_path(cr);
		cairo_rectangle(cr, n->x+n->bb_width-20.0,
			            n->y+n->bb_height-20.0, 20.0, 20.0);
		cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
		cairo_fill(cr);
	}

	if ( ti->drag_reason == IMAGE_DRAG_REASON_RESIZE ) {

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
	/* Do nothing */
}


static void im_commit(struct object *o, gchar *str, struct toolinfo *tip)
{
	/* Do nothing */
}


static int valid_object(struct object *o)
{
	if ( o->type == OBJ_IMAGE ) return 1;
	return 0;
}


static struct object *deserialize(struct presentation *p, struct ds_node *root,
                                  struct toolinfo *tip)
{
	struct image_object *o;
	double x, y, w, h;
	char *filename;
	struct image_toolinfo *ti = (struct image_toolinfo *)tip;

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
	if ( get_field_s(root, "filename", &filename) ) {
		fprintf(stderr, "Couldn't find filename for object '%s'\n",
		        root->key);
		return NULL;
	}

	o = new_image_object(x, y, w, h, filename,
	                     p->ss->styles[0], p->image_store, ti);
	free(filename);

	return (struct object *)o;
}


struct toolinfo *initialise_image_tool()
{
	struct image_toolinfo *ti;

	ti = malloc(sizeof(*ti));

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
	ti->base.deserialize = deserialize;

	return (struct toolinfo *)ti;
}
