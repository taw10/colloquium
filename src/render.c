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

#include <storycode.h>

#include "stylesheet.h"
#include "presentation.h"
#include "frame.h"


struct renderstuff
{
	cairo_t *cr;
	PangoContext *pc;
	PangoFontMap *fontmap;
	PangoFont *font;
	struct frame *fr;
};


static void render_segment(gpointer data, gpointer user_data)
{
	struct renderstuff *s = user_data;
	PangoItem *item = data;
	PangoGlyphString *glyphs;
	GdkColor col;

	glyphs = pango_glyph_string_new();

	pango_shape(s->fr->sc+item->offset, item->length, &item->analysis,
	            glyphs);

	/* FIXME: Honour alpha as well */
	gdk_color_parse("#000000", &col);
	gdk_cairo_set_source_color(s->cr, &col);
	pango_cairo_show_glyph_string(s->cr, s->font, glyphs);

	pango_glyph_string_free(glyphs);
	pango_item_free(item);
}


/* Render Level 1 Storycode */
int render_sc(struct frame *fr, cairo_t *cr, double w, double h)
{
	PangoFontDescription *fontdesc;
	struct renderstuff s;
	GList *list;

	if ( fr->sc == NULL ) return 0;

	s.pc = pango_cairo_create_context(cr);
	s.fr = fr;
	s.cr = cr;

	/* Find and load font */
	s.fontmap = pango_cairo_font_map_get_default();
	fontdesc = pango_font_description_from_string("Serif 20");
	s.font = pango_font_map_load_font(s.fontmap, s.pc, fontdesc);
	if ( s.font == NULL ) {
		fprintf(stderr, "Failed to load font.\n");
		return 1;
	}

	/* Create glyph string */
	list = pango_itemize(s.pc, fr->sc, 0, strlen(fr->sc), NULL, NULL);
	g_list_foreach(list, render_segment, &s);

	g_list_free(list);
	pango_font_description_free(fontdesc);
	g_object_unref(s.fontmap);
	g_object_unref(s.pc);

	return 0;
}


int render_frame(struct frame *fr, cairo_t *cr)
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

			if ( (fr->style != NULL)
			  && (strcmp(fr->style->name, "Default") == 0) )
			{
				cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
			} else {
				cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			}
			cairo_fill_preserve(cr);
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

			/* if draw_border */
			//cairo_set_line_width(cr, 1.0);
			//cairo_stroke(cr);

			cairo_new_path(cr);

			/* Set up padding, and then draw the contents */
			w = fr->w - (fr->lop.pad_l + fr->lop.pad_r);
			h = fr->h - (fr->lop.pad_t + fr->lop.pad_b);

			cairo_move_to(cr, fr->lop.pad_l, fr->lop.pad_t+h);
			render_sc(fr, cr, w, h);

			d = 1;

			continue;

		}

		/* Sort out the transformation for the margins */
		cairo_translate(cr, fr->rendering_order[i]->offs_x,
		                    fr->rendering_order[i]->offs_y);

		render_frame(fr->rendering_order[i], cr);

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

	printf("rendered to %p %ix%i\n", surf, w, h);
	render_frame(s->top, cr);

	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);

	return surf;
}
