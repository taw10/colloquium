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


struct macro
{
	char *name;
	char *sc;
	struct macro *prev;  /* Previous declaration, or NULL */
};


struct sc_state
{
	PangoFontDescription *fontdesc;
	PangoFont *font;
	double col[4];
	int ascent;
	int height;

	struct frame *fr;  /* The current frame */

	int n_macros;
	struct macro *macros;  /* Contents need to be copied on push */
};


struct _scinterp
{
	PangoContext *pc;
	PangoLanguage *lang;

	struct slide_constants *s_constants;
	struct presentation_constants *p_constants;

	struct sc_state *state;
	int j;  /* Index of the current state */
	int max_state;
};


PangoFont *sc_interp_get_font(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->font;
}


PangoFontDescription *sc_interp_get_fontdesc(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->fontdesc;
}


double *sc_interp_get_fgcol(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->col;
}


int sc_interp_get_ascent(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->ascent;
}


int sc_interp_get_height(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->height;
}


static void set_font(SCInterpreter *scin, const char *font_name)
{
	PangoFontMetrics *metrics;
	struct sc_state *st = &scin->state[scin->j];

	st->fontdesc = pango_font_description_from_string(font_name);
	if ( st->fontdesc == NULL ) {
		fprintf(stderr, "Couldn't describe font.\n");
		return;
	}
	st->font = pango_font_map_load_font(pango_context_get_font_map(scin->pc),
	                                    scin->pc, st->fontdesc);
	if ( st->font == NULL ) {
		fprintf(stderr, "Couldn't load font.\n");
		return;
	}

	/* FIXME: Language for box */
	metrics = pango_font_get_metrics(st->font, NULL);
	st->ascent = pango_font_metrics_get_ascent(metrics);
	st->height = st->ascent + pango_font_metrics_get_descent(metrics);
	pango_font_metrics_unref(metrics);
}


/* This sets the colour for the font at the top of the stack */
static void set_colour(SCInterpreter *scin, const char *colour)
{
	GdkRGBA col;
	struct sc_state *st = &scin->state[scin->j];

	if ( colour == NULL ) {
		printf("Invalid colour\n");
		st->col[0] = 0.0;
		st->col[1] = 0.0;
		st->col[2] = 0.0;
		st->col[3] = 1.0;
		return;
	}

	gdk_rgba_parse(&col, colour);

	st->col[0] = col.red;
	st->col[1] = col.green;
	st->col[2] = col.blue;
	st->col[3] = col.alpha;
}


static void set_frame_bgcolour(struct frame *fr, const char *colour)
{
	GdkRGBA col;

	if ( colour == NULL ) {
		printf("Invalid colour\n");
		fr->bgcol[0] = 0.0;
		fr->bgcol[1] = 0.0;
		fr->bgcol[2] = 0.0;
		fr->bgcol[3] = 1.0;
		return;
	}

	gdk_rgba_parse(&col, colour);

	fr->bgcol[0] = col.red;
	fr->bgcol[1] = col.green;
	fr->bgcol[2] = col.blue;
	fr->bgcol[3] = col.alpha;
}


void sc_interp_save(SCInterpreter *scin)
{
	if ( scin->j+1 == scin->max_state ) {

		struct sc_state *stack_new;

		stack_new = realloc(scin->state, sizeof(struct sc_state)
		                     * (scin->max_state+8));
		if ( stack_new == NULL ) {
			fprintf(stderr, "Failed to add to stack.\n");
			return;
		}

		scin->state = stack_new;
		scin->max_state += 8;

	}

	/* When n_fonts=0, we leave the first font uninitialised.  This allows
	 * the stack to be "bootstrapped", but requires the first caller to do
	 * set_font and set_colour straight away. */
	scin->state[scin->j+1] = scin->state[scin->j];
	scin->j++;
}


void sc_interp_restore(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];

	if ( scin->j > 0 ) {
		if ( st->fontdesc != scin->state[scin->j-1].fontdesc )
		{
			pango_font_description_free(st->fontdesc);
		} /* else the font is the same as the previous one, and we
		   * don't need to free it just yet */
	}

	scin->j--;
}


struct frame *sc_interp_get_frame(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->fr;
}


static void set_frame(SCInterpreter *scin, struct frame *fr)
{
	struct sc_state *st = &scin->state[scin->j];
	st->fr = fr;
}


SCInterpreter *sc_interp_new(PangoContext *pc, struct frame *top)
{
	SCInterpreter *scin;

	scin = malloc(sizeof(SCInterpreter));
	if ( scin == NULL ) return NULL;

	scin->state = malloc(8*sizeof(struct sc_state));
	if ( scin->state == NULL ) {
		free(scin);
		return NULL;
	}
	scin->j = 0;
	scin->max_state = 8;

	scin->pc = pc;
	scin->s_constants = NULL;
	scin->p_constants = NULL;

	/* FIXME: Determine proper language (somehow...) */
	scin->lang = pango_language_from_string("en_GB");

	/* The "ultimate" default font */
	set_font(scin, "Sans 12");
	set_colour(scin, "#000000");
	set_frame(scin, top);

	return scin;
}


void sc_interp_destroy(SCInterpreter *scin)
{
	/* Empty the stack */
	while ( scin->j > 0 ) {
		sc_interp_restore(scin);
	}

	pango_font_description_free(scin->state[0].fontdesc);

	free(scin);
}


static LengthUnits get_units(const char *t)
{
	size_t len = strlen(t);

	if ( t[len-1] == 'f' ) return UNITS_FRAC;
	if ( t[len-1] == 'u' ) return UNITS_SLIDE;

	fprintf(stderr, "Invalid units in '%s'\n", t);
	return UNITS_SLIDE;
}


static void parse_frame_option(struct frame *fr, struct frame *parent,
                               const char *opt)
{
	if ( (index(opt, 'x') != NULL) && (index(opt, '+') != NULL)
	  && (index(opt, '+') != rindex(opt, '+')) )
	{
		char *w;
		char *h;
		char *x;
		char *y;
		char *check;
		LengthUnits h_units, w_units;

		/* Looks like a dimension/position thing */
		w = strdup(opt);
		h = index(w, 'x');
		h[0] = '\0';  h++;

		x = index(h, '+');
		if ( x == NULL ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		x[0] = '\0';  x++;

		y = index(x, '+');
		if ( x == NULL ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		y[0] = '\0';  y++;

		fr->w = strtod(w, &check);
		if ( check == w ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		w_units = get_units(w);
		if ( w_units == UNITS_FRAC ) {
			double pw = parent->w;
			pw -= parent->pad_l;
			pw -= parent->pad_r;
			fr->w = pw * fr->w;
		}

		fr->h = strtod(h, &check);
		if ( check == h ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		h_units = get_units(h);
		if ( h_units == UNITS_FRAC ) {
			double ph = parent->h;
			ph -= parent->pad_t;
			ph -= parent->pad_b;
			fr->h = ph * fr->h;
		}

		fr->x = strtod(x, &check);
		if ( check == x ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}
		fr->y = strtod(y, &check);
		if ( check == y ) {
			fprintf(stderr, "Invalid option '%s'\n", opt);
			return;
		}

	}
}


static void parse_frame_options(struct frame *fr, struct frame *parent,
                                const char *opth)
{
	int i;
	size_t len;
	size_t start;
	char *opt;

	if ( opth == NULL ) return;

	opt = strdup(opth);

	len = strlen(opt);
	start = 0;

	for ( i=0; i<len; i++ ) {

		/* FIXME: comma might be escaped or quoted */
		if ( opt[i] == ',' ) {
			opt[i] = '\0';
			parse_frame_option(fr, parent, opt+start);
			start = i+1;
		}

	}

	if ( start != len ) {
		parse_frame_option(fr, parent, opt+start);
	}

	free(opt);
}


static int get_size(const char *a, struct frame *fr, int *wp, int *hp)
{
	char *x;
	char *ws;
	char *hs;

	if ( a == NULL ) goto invalid;

	x = index(a, 'x');
	if ( x == NULL ) goto invalid;

	if ( rindex(a, 'x') != x ) goto invalid;

	ws = strndup(a, x-a);
	hs = strdup(x+1);

	if ( strcmp(ws, "fit") == 0 ) {
		*wp = fr->w - (fr->pad_l+fr->pad_r);
	} else {
		*wp = strtoul(ws, NULL, 10);
	}
	if ( strcmp(ws, "fit") == 0 ) {
		*hp = fr->h - (fr->pad_t+fr->pad_b);
	} else {
		*hp = strtoul(hs, NULL, 10);
	}

	free(ws);
	free(hs);

	return 0;

invalid:
	fprintf(stderr, "Invalid dimensions '%s'\n", a);
	return 1;
}


static int check_outputs(SCBlock *bl, SCInterpreter *scin, int *recurse)
{
	const char *name = sc_block_name(bl);
	const char *options = sc_block_options(bl);
	const char *contents = sc_block_contents(bl);
	SCBlock *child = sc_block_child(bl);

	if ( name == NULL ) {
		split_words(sc_interp_get_frame(scin)->boxes,
		            scin->pc, contents, scin->lang, 1,
		            scin);

	} else if ( strcmp(name, "bgcol") == 0 ) {
		set_frame_bgcolour(sc_interp_get_frame(scin), options);

	} else if ( strcmp(name, "image")==0 ) {
		int w, h;
		if ( get_size(options, sc_interp_get_frame(scin),
		              &w, &h) == 0 )
		{
			add_image_box(sc_interp_get_frame(scin)->boxes,
			              sc_block_contents(child),
			              w, h, 1);
			*recurse = 0;
		}

	} else if ( strcmp(name, "slidenumber")==0) {
		char *tmp = malloc(64);
		if ( tmp != NULL ) {
			snprintf(tmp, 63, "%i",
			         scin->s_constants->slide_number);
			split_words(sc_interp_get_frame(scin)->boxes,
			            scin->pc, tmp, scin->lang, 0, scin);
		}

	} else if ( strcmp(name, "f")==0 ) {

		struct frame *fr = sc_block_frame(bl);

		if ( fr != NULL ) {
			free(fr->boxes->boxes);
			free(fr->boxes);
			fr->boxes = malloc(sizeof(struct wrap_line));
			initialise_line(fr->boxes);
		}

		if ( fr == NULL ) {
			fr = add_subframe(sc_interp_get_frame(scin));
			sc_block_set_frame(bl, fr);
			fr->scblocks = child;
		}
		if ( fr == NULL ) {
			fprintf(stderr, "Failed to add frame.\n");
			return 1;
		}

		parse_frame_options(fr, sc_interp_get_frame(scin),
			            options);
		sc_interp_set_frame(scin, fr);
		*recurse = 1;

	} else {
		return 0;
	}

	return 1;  /* handled */
}


int sc_interp_add_blocks(SCInterpreter *scin, SCBlock *bl)
{
	while ( bl != NULL ) {

		int recurse = 0;
		const char *name = sc_block_name(bl);
		const char *options = sc_block_options(bl);
		const char *contents = sc_block_contents(bl);
		SCBlock *child = sc_block_child(bl);

		if ( child != NULL ) {
			sc_interp_save(scin);
		}

		if ( (sc_interp_get_frame(scin) != NULL)
		  && check_outputs(bl, scin, &recurse) ) {
			/* Block handled as output thing */

		} else if ( name == NULL ) {
			/* Dummy to ensure name != NULL below */

		} else if ( strcmp(name, "font") == 0 ) {
			set_font(scin, options);
			recurse = 1;

		} else if ( strcmp(name, "fgcol") == 0 ) {
			set_colour(scin, options);
			recurse = 1;

		} else {

			fprintf(stderr, "Don't know what to do with this:\n");
			show_sc_block(bl, "");

		}

		if ( child != NULL ) {
			if ( recurse ) sc_interp_add_blocks(scin, child);
			sc_interp_restore(scin);
		}
		bl = sc_block_next(bl);

	}

	return 0;
}

