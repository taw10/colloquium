/*
 * sc_interp.c
 *
 * Copyright © 2014-2018 Thomas White <taw@bitwiz.org.uk>
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

#include "imagestore.h"
#include "sc_parse.h"
#include "sc_interp.h"
#include "presentation.h"
#include "utils.h"


struct macro
{
	char *name;
	SCBlock *bl;
	struct macro *prev;  /* Previous declaration, or NULL */
};


struct template
{
	char *name;
	SCBlock *bl;
};


struct sc_state
{
	PangoFontDescription *fontdesc;
	PangoFont *font;
	PangoAlignment alignment;
	double col[4];
	double bgcol[4];
	double bgcol2[4];
	GradientType bggrad;
	int ascent;
	int height;
	float paraspace[4];

	int have_size;
	double slide_width;
	double slide_height;

	struct frame *fr;  /* The current frame */

	int n_macros;
	int max_macros;
	struct macro *macros;  /* Contents need to be copied on push */

	int n_styles;
	int max_styles;
	struct macro *styles;  /* Contents need to be copied on push */

	int n_templates;
	int max_templates;
	struct template *templates;

	SCBlock *macro_contents;  /* If running a macro, the child block of the caller */
	SCBlock *macro_real_block;  /* If running a macro, the block which called the macro */
	int macro_editable;  /* If running a macro, whether this bit can be edited or not */
};


struct _scinterp
{
	PangoContext *pc;
	PangoLanguage *lang;
	ImageStore *is;

	struct slide_constants *s_constants;
	struct presentation_constants *p_constants;

	struct sc_state *state;
	int j;  /* Index of the current state */
	int max_state;

	SCCallbackList *cbl;
};

struct _sccallbacklist
{
	int n_callbacks;
	int max_callbacks;
	char **names;
	SCCallbackBoxFunc *box_funcs;
	SCCallbackDrawFunc *draw_funcs;
	SCCallbackClickFunc *click_funcs;
	void **vps;
};


SCCallbackList *sc_callback_list_new()
{
	SCCallbackList *cbl;

	cbl = malloc(sizeof(struct _sccallbacklist));
	if ( cbl == NULL ) return NULL;

	cbl->names = calloc(8, sizeof(char *));
	if ( cbl->names == NULL ) return NULL;

	cbl->box_funcs = calloc(8, sizeof(cbl->box_funcs[0]));
	if ( cbl->box_funcs == NULL ) return NULL;

	cbl->draw_funcs = calloc(8, sizeof(cbl->draw_funcs[0]));
	if ( cbl->draw_funcs == NULL ) return NULL;

	cbl->click_funcs = calloc(8, sizeof(cbl->click_funcs[0]));
	if ( cbl->click_funcs == NULL ) return NULL;

	cbl->vps = calloc(8, sizeof(cbl->vps[0]));
	if ( cbl->vps == NULL ) return NULL;

	cbl->max_callbacks = 8;
	cbl->n_callbacks = 0;

	return cbl;
}


void sc_callback_list_free(SCCallbackList *cbl)
{
	int i;

	if ( cbl == NULL ) return;

	for ( i=0; i<cbl->n_callbacks; i++ ) {
		free(cbl->names[i]);
	}

	free(cbl->names);
	free(cbl->box_funcs);
	free(cbl->draw_funcs);
	free(cbl->vps);
	free(cbl);

}


void sc_callback_list_add_callback(SCCallbackList *cbl, const char *name,
                                   SCCallbackBoxFunc box_func,
                                   SCCallbackDrawFunc draw_func,
                                   SCCallbackClickFunc click_func,
                                   void *vp)
{
	if ( cbl->n_callbacks == cbl->max_callbacks ) {

		SCCallbackBoxFunc *box_funcs_new;
		SCCallbackDrawFunc *draw_funcs_new;
		SCCallbackClickFunc *click_funcs_new;
		char **names_new;
		void **vps_new;
		int mcn = cbl->max_callbacks + 8;

		names_new = realloc(cbl->names, mcn*sizeof(char *));
		box_funcs_new = realloc(cbl->box_funcs,
		                        mcn*sizeof(SCCallbackBoxFunc));
		draw_funcs_new = realloc(cbl->draw_funcs,
		                        mcn*sizeof(SCCallbackDrawFunc));
		click_funcs_new = realloc(cbl->click_funcs,
		                        mcn*sizeof(SCCallbackClickFunc));
		vps_new = realloc(cbl->vps, mcn*sizeof(void *));

		if ( (names_new == NULL) || (box_funcs_new == NULL)
		  || (vps_new == NULL) || (draw_funcs_new == NULL)
		  || (click_funcs_new == NULL) ) {
			fprintf(stderr, _("Failed to grow callback list\n"));
			return;
		}

		cbl->names = names_new;
		cbl->box_funcs = box_funcs_new;
		cbl->draw_funcs = draw_funcs_new;
		cbl->click_funcs = click_funcs_new;
		cbl->vps = vps_new;
		cbl->max_callbacks = mcn;

	}

	cbl->names[cbl->n_callbacks] = strdup(name);
	cbl->box_funcs[cbl->n_callbacks] = box_func;
	cbl->draw_funcs[cbl->n_callbacks] = draw_func;
	cbl->click_funcs[cbl->n_callbacks] = click_func;
	cbl->vps[cbl->n_callbacks] = vp;
	cbl->n_callbacks++;
}


void sc_interp_set_callbacks(SCInterpreter *scin, SCCallbackList *cbl)
{
	if ( scin->cbl != NULL ) {
		fprintf(stderr, _("WARNING: Interpreter already has a callback "
		                  "list.\n"));
	}
	scin->cbl = cbl;
}


static int check_callback(SCInterpreter *scin, SCBlock *bl)
{
	int i;
	const char *name = sc_block_name(bl);
	SCCallbackList *cbl = scin->cbl;

	/* No callback list -> easy */
	if ( cbl == NULL ) return 0;

	/* No name -> definitely not a callback */
	if ( name == NULL ) return 0;

	for ( i=0; i<cbl->n_callbacks; i++ ) {

		double w, h;
		int r;
		void *bvp;
		SCBlock *rbl;

		rbl = bl;
		if ( sc_interp_get_macro_real_block(scin) != NULL ) {
			bl = sc_interp_get_macro_real_block(scin);
		}

		if ( strcmp(cbl->names[i], name) != 0 ) continue;
		r = cbl->box_funcs[i](scin, bl, &w, &h, &bvp, cbl->vps[i]);
		if ( r )  {
			struct sc_state *st = &scin->state[scin->j];
			Paragraph *pnew;
			pnew = add_callback_para(sc_interp_get_frame(scin),
			                         bl, rbl, w, h,
			                         cbl->draw_funcs[i],
			                         cbl->click_funcs[i],
			                         bvp, cbl->vps[i]);
			if ( pnew != NULL ) {
				set_para_spacing(pnew, st->paraspace);
			}

		}
		return 1;

	}

	return 0;
}


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


double *sc_interp_get_bgcol(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->bgcol;
}


double *sc_interp_get_bgcol2(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->bgcol2;
}


GradientType sc_interp_get_bggrad(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->bggrad;
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


static void set_frame_default_style(struct frame *fr, SCInterpreter *scin)
{
	if ( fr == NULL ) return;

	if ( fr->fontdesc != NULL ) {
		pango_font_description_free(fr->fontdesc);
	}
	fr->fontdesc = pango_font_description_copy(sc_interp_get_fontdesc(scin));
	fr->col[0] = sc_interp_get_fgcol(scin)[0];
	fr->col[1] = sc_interp_get_fgcol(scin)[1];
	fr->col[2] = sc_interp_get_fgcol(scin)[2];
	fr->col[3] = sc_interp_get_fgcol(scin)[3];
}


static void update_font(SCInterpreter *scin)
{
	PangoFontMetrics *metrics;
	struct sc_state *st = &scin->state[scin->j];

	if ( scin->pc == NULL ) return;

	st->font = pango_font_map_load_font(pango_context_get_font_map(scin->pc),
	                                    scin->pc, st->fontdesc);
	if ( st->font == NULL ) {
		char *f = pango_font_description_to_string(st->fontdesc);
		fprintf(stderr, _("Couldn't load font '%s' (font map %p, pc %p)\n"),
		        f, pango_context_get_font_map(scin->pc), scin->pc);
		g_free(f);
		return;
	}

	/* FIXME: Language for box */
	metrics = pango_font_get_metrics(st->font, NULL);
	st->ascent = pango_font_metrics_get_ascent(metrics);
	st->height = st->ascent + pango_font_metrics_get_descent(metrics);
	pango_font_metrics_unref(metrics);
	set_frame_default_style(sc_interp_get_frame(scin), scin);
}


static void set_font(SCInterpreter *scin, const char *font_name)
{
	struct sc_state *st = &scin->state[scin->j];

	st->fontdesc = pango_font_description_from_string(font_name);
	if ( st->fontdesc == NULL ) {
		fprintf(stderr, _("Couldn't describe font.\n"));
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
		fprintf(stderr, _("Invalid font size '%s'\n"), size_str);
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


static void set_oblique(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	copy_top_fontdesc(scin);
	pango_font_description_set_style(st->fontdesc, PANGO_STYLE_OBLIQUE);
	update_font(scin);
}


static void set_italic(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	copy_top_fontdesc(scin);
	pango_font_description_set_style(st->fontdesc, PANGO_STYLE_ITALIC);
	update_font(scin);
}


static void set_alignment(SCInterpreter *scin, PangoAlignment align)
{
	struct sc_state *st = &scin->state[scin->j];
	st->alignment = align;
}


/* This sets the colour for the font at the top of the stack */
static void set_colour(SCInterpreter *scin, const char *colour)
{
	GdkRGBA col;
	struct sc_state *st = &scin->state[scin->j];

	if ( colour == NULL ) {
		printf(_("Invalid colour\n"));
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
	set_frame_default_style(sc_interp_get_frame(scin), scin);
}


static void update_bg(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	struct frame *fr = sc_interp_get_frame(scin);

	if ( fr == NULL ) return;

	fr->bgcol[0] = st->bgcol[0];
	fr->bgcol[1] = st->bgcol[1];
	fr->bgcol[2] = st->bgcol[2];
	fr->bgcol[3] = st->bgcol[3];
	fr->bgcol2[0] = st->bgcol2[0];
	fr->bgcol2[1] = st->bgcol2[1];
	fr->bgcol2[2] = st->bgcol2[2];
	fr->bgcol2[3] = st->bgcol2[3];
	fr->grad = st->bggrad;
}


static void set_bgcol(SCInterpreter *scin, const char *colour)
{
	GdkRGBA col;
	struct sc_state *st = &scin->state[scin->j];

	if ( colour == NULL ) {
		printf(_("Invalid colour\n"));
		st->bgcol[0] = 0.0;
		st->bgcol[1] = 0.0;
		st->bgcol[2] = 0.0;
		st->bgcol[3] = 1.0;
		return;
	}

	gdk_rgba_parse(&col, colour);

	st->bgcol[0] = col.red;
	st->bgcol[1] = col.green;
	st->bgcol[2] = col.blue;
	st->bgcol[3] = col.alpha;
	st->bggrad = GRAD_NONE;
}


static void set_bggrad(SCInterpreter *scin, const char *options,
                       GradientType grad)
{
	struct sc_state *st = &scin->state[scin->j];
	GdkRGBA col1, col2;
	char *n2;
	char *optcopy = strdup(options);

	if ( options == NULL ) {
		fprintf(stderr, _("Invalid bg gradient spec '%s'\n"), options);
		return;
	}

	n2 = strchr(optcopy, ',');
	if ( n2 == NULL ) {
		fprintf(stderr, _("Invalid bg gradient spec '%s'\n"), options);
		return;
	}

	n2[0] = '\0';

	gdk_rgba_parse(&col1, optcopy);
	gdk_rgba_parse(&col2, &n2[1]);

	st->bgcol[0] = col1.red;
	st->bgcol[1] = col1.green;
	st->bgcol[2] = col1.blue;
	st->bgcol[3] = col1.alpha;

	st->bgcol2[0] = col2.red;
	st->bgcol2[1] = col2.green;
	st->bgcol2[2] = col2.blue;
	st->bgcol2[3] = col2.alpha;

	st->bggrad = grad;

	free(optcopy);
}


void sc_interp_save(SCInterpreter *scin)
{
	if ( scin->j+1 == scin->max_state ) {

		struct sc_state *stack_new;

		stack_new = realloc(scin->state, sizeof(struct sc_state)
		                     * (scin->max_state+8));
		if ( stack_new == NULL ) {
			fprintf(stderr, _("Failed to add to stack.\n"));
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


SCBlock *sc_interp_get_macro_real_block(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	return st->macro_real_block;
}


static void set_frame(SCInterpreter *scin, struct frame *fr)
{
	struct sc_state *st = &scin->state[scin->j];
	st->fr = fr;
}


SCInterpreter *sc_interp_new(PangoContext *pc, PangoLanguage *lang,
                             ImageStore *is, struct frame *top)
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
	scin->is = is;
	scin->s_constants = NULL;
	scin->p_constants = NULL;
	scin->cbl = NULL;

	st = &scin->state[0];
	st->n_macros = 0;
	st->max_macros = 16;
	st->macros = malloc(16*sizeof(struct macro));
	if ( st->macros == NULL ) {
		free(scin->state);
		free(scin);
		return NULL;
	}
	st->n_styles = 0;
	st->max_styles = 16;
	st->styles = malloc(16*sizeof(struct macro));
	if ( st->styles == NULL ) {
		free(scin->state);
		free(scin);
		return NULL;
	}
	st->n_templates = 0;
	st->max_templates = 16;
	st->templates = malloc(16*sizeof(struct template));
	if ( st->templates == NULL ) {
		free(scin->state);
		free(scin);
		return NULL;
	}
	st->macro_contents = NULL;
	st->macro_real_block = NULL;
	st->macro_editable = 0;
	st->fr = NULL;
	st->paraspace[0] = 0.0;
	st->paraspace[1] = 0.0;
	st->paraspace[2] = 0.0;
	st->paraspace[3] = 0.0;
	st->fontdesc = NULL;
	st->have_size = 0;
	st->col[0] = 0.0;
	st->col[1] = 0.0;
	st->col[2] = 0.0;
	st->col[3] = 1.0;
	st->alignment = PANGO_ALIGN_LEFT;
	st->bgcol[0] = 1.0;
	st->bgcol[1] = 1.0;
	st->bgcol[2] = 1.0;
	st->bgcol[3] = 1.0;
	st->bgcol2[0] = 1.0;
	st->bgcol2[1] = 1.0;
	st->bgcol2[2] = 1.0;
	st->bgcol2[3] = 1.0;
	st->bggrad = GRAD_NOBG;
	scin->lang = lang;

	/* The "ultimate" default font */
	if ( scin->pc != NULL ) {
		set_frame(scin, top);
		set_font(scin, "Cantarell Regular 14");
		set_colour(scin, "#000000");
		update_bg(scin);
	}

	return scin;
}


void sc_interp_destroy(SCInterpreter *scin)
{
	/* FIXME: Free all templates and macros */

	/* Empty the stack */
	while ( scin->j > 0 ) {
		sc_interp_restore(scin);
	}

	if ( scin->state[0].fontdesc != NULL ) {
		pango_font_description_free(scin->state[0].fontdesc);
	}

	free(scin->state);
	free(scin);
}


static int parse_double(const char *a, float v[2])
{
	int nn;

	nn = sscanf(a, "%fx%f", &v[0], &v[1]);
	if ( nn != 2 ) {
		fprintf(stderr, _("Invalid size '%s'\n"), a);
		return 1;
	}

	return 0;
}


static int parse_tuple(const char *a, float v[4])
{
	int nn;

	nn = sscanf(a, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
	if ( nn != 4 ) {
		fprintf(stderr, _("Invalid tuple '%s'\n"), a);
		return 1;
	}

	return 0;
}


static void set_padding(struct frame *fr, const char *opts)
{
	float p[4];

	if ( parse_tuple(opts, p) ) return;

	if ( fr == NULL ) return;

	fr->pad_l = p[0];
	fr->pad_r = p[1];
	fr->pad_t = p[2];
	fr->pad_b = p[3];
}


static void set_paraspace(SCInterpreter *scin, const char *opts)
{
	float p[4];
	struct sc_state *st = &scin->state[scin->j];

	if ( parse_tuple(opts, p) ) return;

	st->paraspace[0] = p[0];
	st->paraspace[1] = p[1];
	st->paraspace[2] = p[2];
	st->paraspace[3] = p[3];

	set_para_spacing(last_para(sc_interp_get_frame(scin)), p);
}


static void set_slide_size(SCInterpreter *scin, const char *opts)
{
	float p[2];
	struct sc_state *st = &scin->state[scin->j];

	if ( parse_double(opts, p) ) return;

	st->slide_width = p[0];
	st->slide_height = p[1];
	st->have_size = 1;
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

	fprintf(stderr, _("Invalid units in '%s'\n"), t);
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
		if ( parent != NULL ) {
			double pw = parent->w;
			pw -= parent->pad_l;
			pw -= parent->pad_r;
			*wp = pw * *wp;
		} else {
			*wp = -1.0;
		}

	}

	*hp = strtod(h, &check);
	if ( check == h ) goto invalid;
	h_units = get_units(h);
	if ( h_units == UNITS_FRAC ) {
		if ( parent != NULL ) {
			double ph = parent->h;
			ph -= parent->pad_t;
			ph -= parent->pad_b;
			*hp = ph * *hp;
		} else {
			*hp = -1.0;
		}
	}

	*xp= strtod(x, &check);
	if ( check == x ) goto invalid;
	*yp = strtod(y, &check);
	if ( check == y ) goto invalid;

	return 0;

invalid:
	fprintf(stderr, _("Invalid dimensions '%s'\n"), opt);
	return 1;
}


static int parse_frame_option(const char *opt, struct frame *fr,
                              struct frame *parent)
{
	if ( (index(opt, 'x') != NULL) && (index(opt, '+') != NULL)
	  && (index(opt, '+') != rindex(opt, '+')) ) {
		return parse_dims(opt, parent, &fr->w, &fr->h, &fr->x, &fr->y);
	}

	fprintf(stderr, _("Unrecognised frame option '%s'\n"), opt);

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
		return parse_dims(opt, NULL, wp, hp, &dum, &dum);
	}

	if ( strncmp(opt, "filename=\"", 10) == 0 ) {
		char *fn;
		fn = strdup(opt+10);
		if ( fn[strlen(fn)-1] != '\"' ) {
			fprintf(stderr, _("Unterminated filename?\n"));
			free(fn);
			return 1;
		}
		fn[strlen(fn)-1] = '\0';
		*filenamep = fn;
		return 0;
	}

	fprintf(stderr, _("Unrecognised image option '%s'\n"), opt);

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


static int in_macro(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	if ( st->macro_contents == NULL ) return 0;
	return 1;
}


static void add_newpara(struct frame *fr, SCBlock *bl, SCBlock *mrb)
{
	Paragraph *last_para;

	if ( fr->paras == NULL ) return;
	last_para = fr->paras[fr->n_paras-1];

	set_newline_at_end(last_para, bl);

	/* The block after the \newpara will always be the first one of the
	 * next paragraph, by definition, even if it's \f or another \newpara */
	create_paragraph(fr, sc_block_next(mrb), sc_block_next(bl));
}


/* Add the SCBlock to the text in 'frame', at the end */
static int add_text(struct frame *fr, PangoContext *pc, SCBlock *bl,
                    PangoLanguage *lang, int editable, SCInterpreter *scin)
{
	const char *text = sc_block_contents(bl);
	PangoFontDescription *fontdesc;
	double *col;
	struct sc_state *st = &scin->state[scin->j];
	SCBlock *rbl;
	Paragraph *para;

	/* Empty block? */
	if ( text == NULL ) return 1;

	fontdesc = sc_interp_get_fontdesc(scin);
	col = sc_interp_get_fgcol(scin);

	rbl = bl;
	if ( st->macro_real_block != NULL ) {
		bl = st->macro_real_block;
	}

	para = last_para(fr);
	if ( (para == NULL) || (para_type(para) != PARA_TYPE_TEXT) ) {
		/* Last paragraph is not text.
		 *  or: no paragraphs yet.
		 *    Either way: Create the first one */
		para = create_paragraph(fr, bl, rbl);
	}

	set_para_alignment(para, st->alignment);
	add_run(para, bl, rbl, fontdesc, col);
	set_para_spacing(para, st->paraspace);

	return 0;
}


void sc_interp_run_style(SCInterpreter *scin, const char *sname)
{
	int i;
	struct sc_state *st = &scin->state[scin->j];

	for ( i=0; i<st->n_styles; i++ ) {
		if ( strcmp(sname, st->styles[i].name) == 0 ) {
			sc_interp_add_blocks(scin, st->styles[i].bl);
			return;
		}
	}
}


static int check_outputs(SCBlock *bl, SCInterpreter *scin)
{
	const char *name = sc_block_name(bl);
	const char *options = sc_block_options(bl);
	SCBlock *child = sc_block_child(bl);
	struct sc_state *st = &scin->state[scin->j];

	if ( name == NULL ) {
		add_text(sc_interp_get_frame(scin),
		         scin->pc, bl, scin->lang, 1, scin);

	} else if ( strcmp(name, "image")==0 ) {
		double w, h;
		char *filename;
		if ( parse_image_options(options, sc_interp_get_frame(scin),
		                         &w, &h, &filename) == 0 )
		{
			SCBlock *rbl = bl;
			if ( st->macro_real_block != NULL ) {
				bl = st->macro_real_block;
			}
			add_image_para(sc_interp_get_frame(scin), bl, rbl,
			               filename, scin->is, w, h, 1);
			free(filename);
		} else {
			fprintf(stderr, _("Invalid image options '%s'\n"),
			        options);
		}

	} else if ( strcmp(name, "f")==0 ) {

		struct frame *fr;

		fr = add_subframe(sc_interp_get_frame(scin));
		fr->scblocks = bl;
		if ( in_macro(scin) ) {
			fr->resizable = 0;
		} else {
			fr->resizable = 1;
		}
		if ( fr == NULL ) {
			fprintf(stderr, _("Failed to add frame.\n"));
			return 1;
		}

		set_frame_default_style(fr, scin);

		parse_frame_options(fr, sc_interp_get_frame(scin), options);

		maybe_recurse_before(scin, child);
		set_frame(scin, fr);
		sc_interp_run_style(scin, "frame");
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "newpara")==0 ) {

		struct frame *fr = sc_interp_get_frame(scin);
		SCBlock *rbl = bl;
		if ( st->macro_real_block != NULL ) {
			bl = st->macro_real_block;
		}
		add_newpara(fr, bl, rbl);

	} else {
		return 0;
	}

	return 1;  /* handled */
}


static int check_macro(const char *name, SCInterpreter *scin)
{
	int i;
	struct sc_state *st = &scin->state[scin->j];

	if ( name == NULL ) return 0;

	for ( i=0; i<st->n_macros; i++ ) {
		if ( strcmp(st->macros[i].name, name) == 0 ) {
			return 1;
		}
	}

	return 0;
}


static void exec_macro(SCBlock *bl, SCInterpreter *scin, SCBlock *child)
{
	struct sc_state *st = &scin->state[scin->j];
	int i;
	const char *name;

	name = sc_block_name(bl);
	for ( i=0; i<st->n_macros; i++ ) {
		if ( strcmp(st->macros[i].name, name) == 0 ) {
			sc_interp_save(scin);
			scin->state[scin->j].macro_real_block = bl;
			scin->state[scin->j].macro_contents = child;
			sc_interp_add_blocks(scin, scin->state[scin->j].macros[i].bl);
			sc_interp_restore(scin);
			break; /* Stop iterating, because "st" is now invalid */
		}
	}
}


static void run_macro_contents(SCInterpreter *scin)
{
	struct sc_state *st = &scin->state[scin->j];
	SCBlock *contents = st->macro_contents;

	sc_interp_save(scin);
	scin->state[scin->j].macro_real_block = NULL;
	sc_interp_add_blocks(scin, contents);
	sc_interp_restore(scin);
}


static void run_editable(SCInterpreter *scin, SCBlock *contents)
{
	//struct sc_state *st = &scin->state[scin->j];

	sc_interp_save(scin);
	//scin->state[scin->j].macro_real_block = NULL;
	scin->state[scin->j].macro_editable = 1;
	sc_interp_add_blocks(scin, contents);
	sc_interp_restore(scin);
}


int sc_interp_add_block(SCInterpreter *scin, SCBlock *bl)
{
	const char *name = sc_block_name(bl);
	const char *options = sc_block_options(bl);
	SCBlock *child = sc_block_child(bl);

	//printf("Running this --------->\n");
	//show_sc_blocks(bl);
	//printf("<------------\n");

	if ( check_macro(name, scin) ) {
		sc_interp_save(scin);
		exec_macro(bl, scin, child);
		sc_interp_restore(scin);

	} else if ( check_callback(scin, bl) ) {
		/* Handled in check_callback, don't do anything else */

	} else if ((sc_interp_get_frame(scin) != NULL)
	  && check_outputs(bl, scin) ) {
		/* Block handled as output thing */

	} else if ( name == NULL ) {
		/* Dummy to ensure name != NULL below */

	} else if ( strcmp(name, "presentation") == 0 ) {
		maybe_recurse_before(scin, child);
		sc_interp_run_style(scin, "narrative");
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "stylesheet") == 0 ) {
		/* Ignore (see sc_interp_run_stylesheet)  */

	} else if ( strcmp(name, "slide") == 0 ) {
		maybe_recurse_before(scin, child);
		sc_interp_run_style(scin, "slide");
		maybe_recurse_after(scin, child);

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

	} else if ( strcmp(name, "oblique") == 0 ) {
		maybe_recurse_before(scin, child);
		set_oblique(scin);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "italic") == 0 ) {
		maybe_recurse_before(scin, child);
		set_italic(scin);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "lalign") == 0 ) {
		maybe_recurse_before(scin, child);
		set_alignment(scin, PANGO_ALIGN_LEFT);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "ralign") == 0 ) {
		maybe_recurse_before(scin, child);
		set_alignment(scin, PANGO_ALIGN_RIGHT);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "center") == 0 ) {
		maybe_recurse_before(scin, child);
		set_alignment(scin, PANGO_ALIGN_CENTER);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "fgcol") == 0 ) {
		maybe_recurse_before(scin, child);
		set_colour(scin, options);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "contents") == 0 ) {
		run_macro_contents(scin);

	} else if ( strcmp(name, "editable") == 0 ) {
		run_editable(scin, child);

	} else if ( strcmp(name, "pad") == 0 ) {
		maybe_recurse_before(scin, child);
		set_padding(sc_interp_get_frame(scin), options);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "bgcol") == 0 ) {
		maybe_recurse_before(scin, child);
		set_bgcol(scin, options);
		update_bg(scin);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "bggradh") == 0 ) {
		maybe_recurse_before(scin, child);
		set_bggrad(scin, options, GRAD_HORIZ);
		update_bg(scin);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "bggradv") == 0 ) {
		maybe_recurse_before(scin, child);
		set_bggrad(scin, options, GRAD_VERT);
		update_bg(scin);
		maybe_recurse_after(scin, child);

	} else if ( strcmp(name, "paraspace") == 0 ) {
		maybe_recurse_before(scin, child);
		set_paraspace(scin, options);
		maybe_recurse_after(scin, child);

	} else {

		fprintf(stderr, "Don't know what to do with this:\n");
		show_sc_block(bl, "");

	}

	return 0;
}


int sc_interp_add_blocks(SCInterpreter *scin, SCBlock *bl)
{
	while ( bl != NULL ) {
		if ( sc_interp_add_block(scin, bl) ) return 1;
		bl = sc_block_next(bl);
	}

	return 0;
}


static int try_add_style(SCInterpreter *scin, const char *options, SCBlock *bl)
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

	for ( i=0; i<st->n_styles; i++ ) {
		if ( strcmp(st->styles[i].name, nn) == 0 ) {
			st->styles[i].name = nn;
			st->styles[i].bl = bl;
			st->styles[i].prev = NULL;  /* FIXME: Stacking */
			return 0;
		}
	}

	if ( st->max_styles == st->n_styles ) {

		struct macro *styles_new;

		styles_new = realloc(st->styles, sizeof(struct macro)
		                     * (st->max_styles+16));
		if ( styles_new == NULL ) {
			fprintf(stderr, _("Failed to add style.\n"));
			return 1;
		}

		st->styles = styles_new;
		st->max_styles += 16;

	}

	i = st->n_styles++;

	st->styles[i].name = nn;
	st->styles[i].bl = bl;
	st->styles[i].prev = NULL;  /* FIXME: Stacking */

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
			fprintf(stderr, _("Failed to add macro.\n"));
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


static int try_add_template(SCInterpreter *scin, const char *options, SCBlock *bl)
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

	for ( i=0; i<st->n_templates; i++ ) {
		if ( strcmp(st->templates[i].name, nn) == 0 ) {
			fprintf(stderr, _("Duplicate template '%s'\n"), nn);
			return 0;
		}
	}

	if ( st->max_templates == st->n_templates ) {

		struct template *templates_new;

		templates_new = realloc(st->templates, sizeof(struct template)
		                     * (st->max_templates+16));
		if ( templates_new == NULL ) {
			fprintf(stderr, _("Failed to add templates\n"));
			return 1;
		}

		st->templates = templates_new;
		st->max_templates += 16;

	}

	i = st->n_templates++;

	st->templates[i].name = nn;
	st->templates[i].bl = bl;

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
		fprintf(stderr, _("Style sheet isn't a style sheet.\n"));
		return;
	}

	bl = sc_block_child(bl);

	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);
		const char *options = sc_block_options(bl);

		if ( name == NULL ) {

			/* Do nothing */

		} else if ( strcmp(name, "def") == 0 ) {
			try_add_macro(scin, options, sc_block_child(bl));

		} else if ( strcmp(name, "ss") == 0 ) {  /* Backward compatibility */
			try_add_macro(scin, options, sc_block_child(bl));

		} else if ( strcmp(name, "style") == 0 ) {
			try_add_style(scin, options, sc_block_child(bl));

		} else if ( strcmp(name, "template") == 0 ) {
			try_add_template(scin, options, sc_block_child(bl));

		} else if ( strcmp(name, "font") == 0 ) {
			set_font(scin, options);

		} else if ( strcmp(name, "fgcol") == 0 ) {
			set_colour(scin, options);

		} else if ( strcmp(name, "bgcol") == 0 ) {
			set_bgcol(scin, options);
			update_bg(scin);

		} else if ( strcmp(name, "bggradh") == 0 ) {
			set_bggrad(scin, options, GRAD_HORIZ);
			update_bg(scin);

		} else if ( strcmp(name, "bggradv") == 0 ) {
			set_bggrad(scin, options, GRAD_VERT);
			update_bg(scin);

		} else if ( strcmp(name, "paraspace") == 0 ) {
			set_paraspace(scin, options);

		} else if ( strcmp(name, "slidesize") == 0 ) {
			set_slide_size(scin, options);

		}

		bl = sc_block_next(bl);

	}
}


int sc_interp_get_slide_size(SCInterpreter *scin, double *w, double *h)
{
	if ( !scin->state->have_size ) return 1;
	*w = scin->state->slide_width;
	*h = scin->state->slide_height;
	return 0;
}


struct template_id *sc_interp_get_templates(SCInterpreter *scin, int *np)
{
	struct template_id *list;
	int i;

	list = malloc(sizeof(struct template_id)*scin->state->n_templates);
	if ( list == NULL ) return NULL;

	for ( i=0; i<scin->state->n_templates; i++ ) {
		list[i].name = strdup(scin->state->templates[i].name);
		list[i].friendlyname = strdup(scin->state->templates[i].name);
		list[i].scblock = sc_block_copy(scin->state->templates[i].bl);
	}

	*np = scin->state->n_templates;
	return list;
}
