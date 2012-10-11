/*
 * render.c
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
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <assert.h>
#include <gdk/gdk.h>
#include <string.h>

#include "storycode.h"
#include "stylesheet.h"
#include "presentation.h"
#include "frame.h"


/* Render Level 1 Storycode */
int render_sc(struct frame *fr, cairo_t *cr, double w, double h,
              PangoContext *pc)
{
	GdkColor col;

	if ( fr->pl != NULL ) {

		pango_cairo_update_layout(cr, fr->pl);

		/* FIXME: Honour alpha as well */
		gdk_color_parse("#000000", &col);
		gdk_cairo_set_source_color(cr, &col);

		pango_cairo_show_layout(cr, fr->pl);

		//g_object_unref(G_OBJECT(layout));
	}

	return 0;
}


int render_frame(struct frame *fr, cairo_t *cr, PangoContext *pc)
{
	int i;
	int d = 0;

	/* The rendering order is a list of children, but it also contains the
	 * current frame.  In this way, it contains information about which
	 * layer the current frame is to be rendered as relative to its
	 * children. */
	for ( i=0; i<fr->num_ro; i++ ) {

		if ( fr->rendering_order[i] == fr ) {

			double w, h;

			/* Draw the frame itself (rectangle) */
			cairo_rectangle(cr, 0.0, 0.0, fr->w, fr->h);
			cairo_set_line_width(cr, 1.0);

			if ( (fr->style != NULL)
			  && (strcmp(fr->style->name, "Default") == 0) )
			{
				cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
			} else {
				cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			}
			cairo_fill_preserve(cr);
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
			cairo_stroke(cr);

			/* Set up padding, and then draw the contents */
			cairo_move_to(cr, fr->lop.pad_l, fr->lop.pad_t);
			w = fr->w - (fr->lop.pad_l + fr->lop.pad_r);
			h = fr->h - (fr->lop.pad_t + fr->lop.pad_b);
			render_sc(fr, cr, w, h, pc);

			d = 1;

			continue;

		}

		/* Sort out the transformation for the margins */
		cairo_translate(cr, fr->rendering_order[i]->offs_x,
		                    fr->rendering_order[i]->offs_y);

		render_frame(fr->rendering_order[i], cr, pc);

	}

	if ( !d ) {
		fprintf(stderr, "WARNING: Frame didn't appear on its own "
		                "rendering list?\n");
	}

	return 0;
}


cairo_surface_t *render_slide(struct slide *s, int w, int h)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	cairo_font_options_t *fopts;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_OFF);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_font_options(cr, fopts);

	//render_frame(s->top, cr, NULL); /* FIXME: pc */

	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);

	return surf;
}
