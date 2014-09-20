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
	SCBlock *bl;
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
	int max_macros;
	struct macro *macros;  /* Contents need to be copied on push */

	SCBlock *macro_contents;
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


static void update_font(SCInterpreter *scin)
{
	PangoFontMetrics *metrics;
	struct sc_state *st = &scin->state[scin->j];

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


static void set_font(SCInterpreter *scin, const char *font_name)
{
	struct sc_state *st = &scin->state[scin->j];

	st->fontdesc = pango_font_description_from_string(font_name);
	if ( st->fontdesc == NULL ) {
		fprintf(stderr, "Couldn't describe font.\n");
		return;
	}

	update_font(scin);
}


static void copy_top_fontdesc(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];

	/* If this is the first stack frame, don't even check */
	if ( scin->j == 0 ) return;

	/* If the fontdesc at the top of the stack is the same as the one
	 * below, make a copy because we're about to do something to it (which
	 * should not affect the next level up). */
	if ( st->fontdesc == scin->state[scin->j-1].fontdesc ) {
		st->fontdesc = pango_font_description_copy(st->fontdesc);
	}
}


static void set_fontsize(SCInterpreter *scin, const char *size_str)
{
	struct sc_state *st = &scin->state[scin->j];
	int size;
	char *end;

	if ( size_str[0] == '\0' ) return;

	size = strtoul(size_str, &end, 10);
	if ( end[0] != '\0' ) {
		fprintf(stderr, "Invalid font size '%s'\n", size_str);
		return;
	}

	copy_top_fontdesc(scin);
	pango_font_description_set_size(st->fontdesc, size*PANGO_SCALE);
	update_font(scin);
}


static void set_bold(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	copy_top_fontdesc(scin);
	pango_font_description_set_weight(st->fontdesc, PANGO_WEIGHT_BOLD);
	update_font(scin);
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

	if ( fr == NULL ) return;

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
	struct sc_state *st;

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

	st = &scin->state[0];
	st->n_macros = 0;
	st->max_macros = 16;
	st->macros = malloc(16*sizeof(struct macro));
	if ( st->macros == NULL ) {
		free(scin->state);
		free(scin);
		return NULL;
	}
	st->macro_contents = NULL;

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


static void set_padding(struct frame *fr, const char *opts)
{
	int nn;
	float l, r, t, b;

	nn = sscanf(opts, "%f,%f,%f,%f", &l, &r, &t, &b);
	if ( nn != 4 ) {
		fprintf(stderr, "Invalid padding '%s'\n", opts);
		return;
	}

	fr->pad_l = l;
	fr->pad_r = r;
	fr->pad_t = t;
	fr->pad_b = b;
}


void update_geom(struct frame *fr)
{
	char geom[256];
	snprintf(geom, 255, "%.1fux%.1fu+%.1f+%.1f",
	         fr->w, fr->h, fr->x, fr->y);

	/* FIXME: What if there are other options? */
	sc_block_set_options(fr->scblocks, strdup(geom));
}


static LengthUnits get_units(const char *t)
{
	size_t len = strlen(t);

	if ( t[len-1] == 'f' ) return UNITS_FRAC;
	if ( t[len-1] == 'u' ) return UNITS_SLIDE;

	fprintf(stderr, "Invalid units in '%s'\n", t);
	return UNITS_SLIDE;
}


static int parse_dims(const char *opt, struct frame *parent,
                      double *wp, double *hp, double *xp, double *yp)
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
	if ( x == NULL ) goto invalid;
	x[0] = '\0';  x++;

	y = index(x, '+');
	if ( x == NULL ) goto invalid;
	y[0] = '\0';  y++;

	*wp = strtod(w, &check);
	if ( check == w ) goto invalid;
	w_units = get_units(w);
	if ( w_units == UNITS_FRAC ) {
		double pw = parent->w;
		pw -= parent->pad_l;
		pw -= parent->pad_r;
		*wp = pw * *wp;
	}

	*hp = strtod(h, &check);
	if ( check == h ) goto invalid;
	h_units = get_units(h);
	if ( h_units == UNITS_FRAC ) {
		double ph = parent->h;
		ph -= parent->pad_t;
		ph -= parent->pad_b;
		*hp = ph * *hp;
	}

	*xp= strtod(x, &check);
	if ( check == x ) goto invalid;
	*yp = strtod(y, &check);
	if ( check == y ) goto invalid;

	return 0;

invalid:
	fprintf(stderr, "Invalid dimensions '%s'\n", opt);
	return 1;
}


static int parse_frame_option(const char *opt, struct frame *fr,
                              struct frame *parent)
{
	if ( (index(opt, 'x') != NULL) && (index(opt, '+') != NULL)
	  && (index(opt, '+') != rindex(opt, '+')) ) {
		return parse_dims(opt, parent, &fr->w, &fr->h, &fr->x, &fr->y);
	}

	fprintf(stderr, "Unrecognised frame option '%s'\n", opt);

	return 1;
}


static int parse_frame_options(struct frame *fr, struct frame *parent,
                               const char *opth)
{
	int i;
	size_t len;
	size_t start;
	char *opt;

	if ( opth == NULL ) return 1;

	opt = strdup(opth);

	len = strlen(opt);
	start = 0;

	for ( i=0; i<len; i++ ) {

		/* FIXME: comma might be escaped or quoted */
		if ( opt[i] == ',' ) {
			opt[i] = '\0';
			if ( parse_frame_option(opt+start, fr, parent) ) {
				return 1;
			}
			start = i+1;
		}

	}

	if ( start != len ) {
		if ( parse_frame_option(opt+start, fr, parent) ) return 1;
	}

	free(opt);

	return 0;
}


static int parse_image_option(const char *opt, struct frame *parent,
                              double *wp, double *hp, char **filenamep)
{
	if ( (index(opt, 'x') != NULL) && (index(opt, '+') != NULL)
	  && (index(opt, '+') != rindex(opt, '+')) ) {
		double dum;
		return parse_dims(opt, parent, wp, hp, &dum, &dum);
	}

	if ( strncmp(opt, "filename=\"", 10) == 0 ) {
		char *fn;
		fn = strdup(opt+10);
		if ( fn[strlen(fn)-1] != '\"' ) {
			fprintf(stderr, "Unterminated filename?\n");
			free(fn);
			return 1;
		}
		fn[strlen(fn)-1] = '\0';
		*filenamep = fn;
		return 0;
	}

	fprintf(stderr, "Unrecognised image option '%s'\n", opt);

	return 1;
}


static int parse_image_options(const char *opth, struct frame *parent,
                               double *wp, double *hp, char **filenamep)
{
	int i;
	size_t len;
	size_t start;
	char *opt;

	if ( opth == NULL ) return 1;

	opt = strdup(opth);

	len = strlen(opt);
	start = 0;

	for ( i=0; i<len; i++ ) {

		/* FIXME: comma might be escaped or quoted */
		if ( opt[i] == ',' ) {
			opt[i] = '\0';
			if ( parse_image_option(opt+start, parent,
			                        wp, hp, filenamep) ) return 1;
			start = i+1;
		}

	}

	if ( start != len ) {
		if ( parse_image_option(opt+start, parent,
		                        wp, hp, filenamep) ) return 1;
	}

	free(opt);

	return 0;
}


static void maybe_recurse_before(SCInterpreter *scin, SCBlock *child)
{
	if ( child == NULL ) return;

	sc_interp_save(scin);
}


static void maybe_recurse_after(SCInterpreter *scin, SCBlock *child)
{
	if ( child == NULL ) return;

	sc_interp_add_blocks(scin, child);
	sc_interp_restore(scin);
}


static int check_outputs(SCBlock *bl, SCInterpreter *scin)
{
	const char *name = sc_block_name(bl);
	const char *options = sc_block_options(bl);
	const char *contents = sc_block_contents(bl);
	SCBlock *child = sc_block_child(bl);

	if ( name == NULL ) {
		split_words(sc_interp_get_frame(scin)->boxes,
		            scin->pc, bl, contents, scin->lang, 1,
		            scin);

	} else if ( strcmp(name, "bgcol") == 0 ) {
		maybe_recurse_before(scin, child);
		set_frame_bgcolour(sc_interp_get_frame(scin), options);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "image")==0 ) {
		double w, h;
		char *filename;
		if ( parse_image_options(options, sc_interp_get_frame(scin),
		                         &w, &h, &filename) == 0 )
		{
			add_image_box(sc_interp_get_frame(scin)->boxes,
			              filename, w, h, 1);
			free(filename);
		} else {
			fprintf(stderr, "Invalid image options '%s'\n",
			        options);
		}

	} else if ( strcmp(name, "f")==0 ) {

		struct frame *fr = sc_block_frame(bl);

		renew_frame(fr);

		if ( fr == NULL ) {
			fr = add_subframe(sc_interp_get_frame(scin));
			sc_block_set_frame(bl, fr);
			fr->scblocks = bl;
		}
		if ( fr == NULL ) {
			fprintf(stderr, "Failed to add frame.\n");
			return 1;
		}

		parse_frame_options(fr, sc_interp_get_frame(scin),
			            options);

		maybe_recurse_before(scin, child);
		set_frame(scin, fr);
		maybe_recurse_after(scin, child);

	} else {
		return 0;
	}

	return 1;  /* handled */
}


static int check_macro(const char *name, SCInterpreter *scin)
{
	int i;
	struct sc_state *st = &scin->state[scin->j];

	for ( i=0; i<st->n_macros; i++ ) {
		if ( strcmp(st->macros[i].name, name) == 0 ) {
			return 1;
		}
	}

	return 0;
}


static void exec_macro(SCBlock *bl, SCInterpreter *scin, SCBlock *child)
{
	SCBlock *mchild;
	struct sc_state *st = &scin->state[scin->j];

	st->macro_contents = child;

	mchild = sc_block_macro_child(bl);
	if ( mchild == NULL ) {

		int i;
		const char *name;

		/* Copy macro blocks into structure */
		name = sc_block_name(bl);
		for ( i=0; i<st->n_macros; i++ ) {
			if ( strcmp(st->macros[i].name, name) == 0 ) {
				mchild = sc_block_copy(st->macros[i].bl);
				sc_block_set_macro_child(bl, mchild);
			}
		}

	}

	sc_interp_add_blocks(scin, mchild);
}


static void run_macro_contents(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];

	sc_interp_save(scin);
	sc_interp_add_blocks(scin, st->macro_contents);
	sc_interp_restore(scin);
}


int sc_interp_add_blocks(SCInterpreter *scin, SCBlock *bl)
{
	//printf("Running this --------->\n");
	//show_sc_blocks(bl);
	//printf("<------------\n");

	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);
		const char *options = sc_block_options(bl);
		SCBlock *child = sc_block_child(bl);

		if ((sc_interp_get_frame(scin) != NULL)
		  && check_outputs(bl, scin) ) {
			/* Block handled as output thing */

		} else if ( name == NULL ) {
			/* Dummy to ensure name != NULL below */

		} else if ( strcmp(name, "font") == 0 ) {
			maybe_recurse_before(scin, child);
			set_font(scin, options);
			maybe_recurse_after(scin, child);

		} else if ( strcmp(name, "fontsize") == 0 ) {
			maybe_recurse_before(scin, child);
			set_fontsize(scin, options);
			maybe_recurse_after(scin, child);

		} else if ( strcmp(name, "bold") == 0 ) {
			maybe_recurse_before(scin, child);
			set_bold(scin);
			maybe_recurse_after(scin, child);

		} else if ( strcmp(name, "fgcol") == 0 ) {
			maybe_recurse_before(scin, child);
			set_colour(scin, options);
			maybe_recurse_after(scin, child);

		} else if ( check_macro(name, scin) ) {
			sc_interp_save(scin);
			exec_macro(bl, scin, child);
			sc_interp_restore(scin);

		} else if ( strcmp(name, "contents") == 0 ) {
			run_macro_contents(scin);

		} else if ( strcmp(name, "pad") == 0 ) {
			maybe_recurse_before(scin, child);
			set_padding(sc_interp_get_frame(scin), options);
			maybe_recurse_after(scin, child);

		} else if ( strcmp(name, "slide") == 0 ) {
			maybe_recurse_before(scin, child);
			maybe_recurse_after(scin, child);

		} else {

			//fprintf(stderr, "Don't know what to do with this:\n");
			//show_sc_block(bl, "");

		}

		bl = sc_block_next(bl);

	}

	return 0;
}


static int try_add_macro(SCInterpreter *scin, const char *options, SCBlock *bl)
{
	struct sc_state *st = &scin->state[scin->j];
	char *nn;
	char *comma;
	int i;

	nn = strdup(options);
	comma = strchr(nn, ',');
	if ( comma != NULL ) {
		comma[0] = '\0';
	}

	for ( i=0; i<st->n_macros; i++ ) {
		if ( strcmp(st->macros[i].name, nn) == 0 ) {
			st->macros[i].name = nn;
			st->macros[i].bl = bl;
			st->macros[i].prev = NULL;  /* FIXME: Stacking */
			return 0;
		}
	}

	if ( st->max_macros == st->n_macros ) {

		struct macro *macros_new;

		macros_new = realloc(st->macros, sizeof(struct macro)
		                     * (st->max_macros+16));
		if ( macros_new == NULL ) {
			fprintf(stderr, "Failed to add macro.\n");
			return 1;
		}

		st->macros = macros_new;
		st->max_macros += 16;

	}

	i = st->n_macros++;

	st->macros[i].name = nn;
	st->macros[i].bl = bl;
	st->macros[i].prev = NULL;  /* FIXME: Stacking */

	return 0;
}


void add_macro(SCInterpreter *scin, const char *mname, const char *contents)
{
	SCBlock *bl = sc_parse(contents);
	try_add_macro(scin, mname, bl);
}


void sc_interp_run_stylesheet(SCInterpreter *scin, SCBlock *bl)
{
	if ( bl == NULL ) return;

	if ( strcmp(sc_block_name(bl), "stylesheet") != 0 ) {
		fprintf(stderr, "Style sheet isn't a style sheet.\n");
		return;
	}

	bl = sc_block_child(bl);

	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);
		const char *options = sc_block_options(bl);

		if ( (name != NULL) && (strcmp(name, "ss") == 0) ) {
			try_add_macro(scin, options, sc_block_child(bl));

		} else if ( name == NULL ) {

		} else if ( strcmp(name, "font") == 0 ) {
			set_font(scin, options);

		} else if ( strcmp(name, "fgcol") == 0 ) {
			set_colour(scin, options);

		} else if ( strcmp(name, "bgcol") == 0 ) {
			set_frame_bgcolour(sc_interp_get_frame(scin), options);
		}

		bl = sc_block_next(bl);

	}
}


void find_stylesheet(struct presentation *p)
{
	SCBlock *bl = p->scblocks;

	if ( p->stylesheet != NULL ) {
		fprintf(stderr, "Duplicate style sheet!\n");
		return;
	}

	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);

		if ( (name != NULL) && (strcmp(name, "stylesheet") == 0) ) {
			p->stylesheet = bl;
			return;
		}

		bl = sc_block_next(bl);

	}

	fprintf(stderr, "No style sheet.\n");
}


struct style_id *list_styles(SCInterpreter *scin, int *np)
{
	struct style_id *list;
	int i;

	list = malloc(sizeof(struct style_id)*scin->state->n_macros);
	if ( list == NULL ) return NULL;

	for ( i=0; i<scin->state->n_macros; i++ ) {
		list[i].name = strdup(scin->state->macros[i].name);
		list[i].friendlyname = strdup(scin->state->macros[i].name);
	}

	*np = scin->state->n_macros;
	return list;
}
