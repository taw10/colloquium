/*
 * sc_interp.c
 *
 * Copyright © 2014 Thomas White <taw@bitwiz.org.uk>
 *
 * This file is part of Colloquium.
 *
 * Colloquium is free software: you can redistribute it and/or modify
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>
#include <gdk/gdk.h>

#include "sc_parse.h"
#include "sc_interp.h"
#include "shape.h"
#include "wrap.h"

struct _scinterp
{
	PangoContext *pc;
	PangoLanguage *lang;

	struct slide_constants *s_constants;
	struct presentation_constants *p_constants;

	struct sc_font *fontstack;
	int n_fonts;
	int max_fonts;

	struct wrap_line *boxes;
};


static void set_font(SCInterpreter *scin, const char *font_name)
{
	PangoFontMetrics *metrics;
	struct sc_font *scf;

	scf = &scin->fontstack[scin->n_fonts-1];

	scf->fontdesc = pango_font_description_from_string(font_name);
	if ( scf->fontdesc == NULL ) {
		fprintf(stderr, "Couldn't describe font.\n");
		return;
	}
	scf->font = pango_font_map_load_font(pango_context_get_font_map(scin->pc),
	                                     scin->pc, scf->fontdesc);
	if ( scf->font == NULL ) {
		fprintf(stderr, "Couldn't load font.\n");
		return;
	}

	/* FIXME: Language for box */
	metrics = pango_font_get_metrics(scf->font, NULL);
	scf->ascent = pango_font_metrics_get_ascent(metrics);
	scf->height = scf->ascent + pango_font_metrics_get_descent(metrics);
	pango_font_metrics_unref(metrics);

	scf->free_font_on_pop = 1;
}


/* This sets the colour for the font at the top of the stack */
static void set_colour(SCInterpreter *scin, const char *colour)
{
	GdkRGBA col;
	struct sc_font *scf = &scin->fontstack[scin->n_fonts-1];

	if ( colour == NULL ) {
		printf("Invalid colour\n");
		scf->col[0] = 0.0;
		scf->col[1] = 0.0;
		scf->col[2] = 0.0;
		scf->col[3] = 1.0;
		return;
	}

	gdk_rgba_parse(&col, colour);

	scf->col[0] = col.red;
	scf->col[1] = col.green;
	scf->col[2] = col.blue;
	scf->col[3] = col.alpha;
}


static void copy_top_font(SCInterpreter *scin)
{
	if ( scin->n_fonts == scin->max_fonts ) {

		struct sc_font *stack_new;

		stack_new = realloc(scin->fontstack, sizeof(struct sc_font)
		                     * ((scin->max_fonts)+8));
		if ( stack_new == NULL ) {
			fprintf(stderr, "Failed to push font or colour.\n");
			return;
		}

		scin->fontstack = stack_new;
		scin->max_fonts += 8;

	}

	/* When n_fonts=0, we leave the first font uninitialised.  This allows
	 * the stack to be "bootstrapped", but requires the first caller to do
	 * set_font and set_colour straight away. */
	if ( scin->n_fonts > 0 ) {
		scin->fontstack[scin->n_fonts] = scin->fontstack[scin->n_fonts-1];
	}

	/* This is a copy, so don't free it later */
	scin->fontstack[scin->n_fonts].free_font_on_pop = 0;

	scin->n_fonts++;
}


static void push_font(SCInterpreter *scin, const char *font_name)
{
	copy_top_font(scin);
	set_font(scin, font_name);
}


static void push_colour(SCInterpreter *scin, const char *colour)
{
	copy_top_font(scin);
	set_colour(scin, colour);
}


static void pop_font_or_colour(SCInterpreter *scin)
{
	struct sc_font *scf = &scin->fontstack[scin->n_fonts-1];

	if ( scf->free_font_on_pop ) {
		pango_font_description_free(scf->fontdesc);
	}

	scin->n_fonts--;
}


SCInterpreter *sc_interp_new(PangoContext *pc)
{
	SCInterpreter *scin;

	scin = malloc(sizeof(SCInterpreter));
	if ( scin == NULL ) return NULL;

	scin->fontstack = NULL;
	scin->n_fonts = 0;
	scin->max_fonts = 0;

	scin->pc = pc;
	scin->s_constants = NULL;
	scin->p_constants = NULL;

	/* FIXME: Determine proper language (somehow...) */
	scin->lang = pango_language_from_string("en_GB");

	scin->boxes = malloc(sizeof(struct wrap_line));
	if ( scin->boxes == NULL ) {
		fprintf(stderr, "Failed to allocate boxes.\n");
		return NULL;
	}
	initialise_line(scin->boxes);

	/* The "ultimate" default font */
	push_font(scin, "Sans 12");
	set_colour(scin, "#000000");

	return scin;
}


void sc_interp_destroy(SCInterpreter *scin)
{
	/* Empty the stack */
	while ( scin->n_fonts > 0 ) {
		pop_font_or_colour(scin);
	}

	free(scin);
}


int sc_interp_add_blocks(SCInterpreter *scin, const SCBlock *bl)
{
	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);
		const char *options = sc_block_options(bl);
		const char *contents = sc_block_contents(bl);
		SCBlock *child = sc_block_child(bl);

		if ( name == NULL ) {
			split_words(scin->boxes, scin->pc, contents,
			            scin->lang, 1,
			            &scin->fontstack[scin->n_fonts-1]);

		} else if ( (strcmp(name, "font")==0) && (child == NULL) ) {
			set_font(scin, options);

		} else if ( (strcmp(name, "font")==0) && (child != NULL) ) {
			push_font(scin, options);
			sc_interp_add_blocks(scin, child);
			pop_font_or_colour(scin);

		} else if ( (strcmp(name, "fgcol")==0) && (child == NULL) ) {
			set_colour(scin, options);

		} else if ( (strcmp(name, "fgcol")==0) && (child != NULL) ) {
			push_colour(scin, options);
			sc_interp_add_blocks(scin, child);
			pop_font_or_colour(scin);

#if 0
		} else if ( (strcmp(name, "image")==0)
		         && (contents != NULL) && (b->options != NULL) ) {
			int w, h;
			if ( get_size(b->options, fr, &w, &h) == 0 ) {
				add_image_box(boxes, b->contents, offset, w, h,
				              editable);
			}

		} else if ( strcmp(name, "slidenumber")==0) {
			char *tmp = malloc(64);
			if ( tmp != NULL ) {
				snprintf(tmp, 63, "%i",
				         slide_constants->slide_number);
				add_wrap_box(boxes, tmp, offset,
					     WRAP_SPACE_NONE, pc,
					     &fonts->stack[fonts->n_fonts-1],
			                     0);
			} /* else go away and sulk about it */
#endif

		}

		bl = sc_block_next(bl);

	}

	return 0;
}


struct wrap_line *sc_interp_get_boxes(SCInterpreter *scin)
{
	return scin->boxes;
}
