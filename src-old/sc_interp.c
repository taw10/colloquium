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


struct sc_state
{
	PangoFontDescription *fontdesc;
	PangoFont *font;
	PangoAlignment alignment;
	double col[4];
	int ascent;
	int height;
	float paraspace[4];
	char *constants[NUM_SC_CONSTANTS];

	struct frame *fr;  /* The current frame */
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


static int sc_interp_add_blocks(SCInterpreter *scin, SCBlock *bl, Stylesheet *ss)
{
	while ( bl != NULL ) {
		if ( sc_interp_add_block(scin, bl, ss) ) return 1;
		bl = sc_block_next(bl);
	}

	return 0;
}


SCCallbackList *sc_callback_list_new()
{
	SCCallbackList *cbl;

	cbl = malloc(sizeof(struct _sccallbacklist));
	if ( cbl == NULL ) return NULL;

	cbl->names = calloc(8, sizeof(char *));
	if ( cbl->names == NULL ) {
		free(cbl);
		return NULL;
	}

	cbl->box_funcs = calloc(8, sizeof(cbl->box_funcs[0]));
	if ( cbl->box_funcs == NULL ) {
		free(cbl->names);
		free(cbl);
		return NULL;
	}

	cbl->draw_funcs = calloc(8, sizeof(cbl->draw_funcs[0]));
	if ( cbl->draw_funcs == NULL ) {
		free(cbl->box_funcs);
		free(cbl->names);
		free(cbl);
		return NULL;
	}

	cbl->click_funcs = calloc(8, sizeof(cbl->click_funcs[0]));
	if ( cbl->click_funcs == NULL ) {
		free(cbl->draw_funcs);
		free(cbl->box_funcs);
		free(cbl->names);
		free(cbl);
		return NULL;
	}

	cbl->vps = calloc(8, sizeof(cbl->vps[0]));
	if ( cbl->vps == NULL ) {
		free(cbl->click_funcs);
		free(cbl->draw_funcs);
		free(cbl->box_funcs);
		free(cbl->names);
		free(cbl);
		return NULL;
	}

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
			fprintf(stderr, "Failed to grow callback list\n");
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
		fprintf(stderr, "WARNING: Interpreter already has a callback "
		                  "list.\n");
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

		if ( strcmp(cbl->names[i], name) != 0 ) continue;
		r = cbl->box_funcs[i](scin, bl, &w, &h, &bvp, cbl->vps[i]);
		if ( r )  {
			struct sc_state *st = &scin->state[scin->j];
			Paragraph *pnew;
			pnew = add_callback_para(sc_interp_get_frame(scin),
			                         bl, w, h,
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
		fprintf(stderr, "Couldn't load font '%s' (font map %p, pc %p)\n",
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


static void set_bgcol(SCInterpreter *scin, const char *colour)
{
	GdkRGBA col;
	struct frame *fr = sc_interp_get_frame(scin);

	if ( fr == NULL ) return;

	if ( colour == NULL ) {
		printf(_("Invalid colour\n"));
		return;
	}

	gdk_rgba_parse(&col, colour);

	fr->bgcol[0] = col.red;
	fr->bgcol[1] = col.green;
	fr->bgcol[2] = col.blue;
	fr->bgcol[3] = col.alpha;
	fr->grad = GRAD_NONE;
}


static void set_bggrad(SCInterpreter *scin, const char *options,
                       GradientType grad)
{
	struct frame *fr = sc_interp_get_frame(scin);
	GdkRGBA col1, col2;

	if ( fr == NULL ) return;

	if ( options == NULL ) {
		printf(_("Invalid colour\n"));
		return;
	}

	if ( parse_colour_duo(options, &col1, &col2) == 0 ) {

		fr->bgcol[0] = col1.red;
		fr->bgcol[1] = col1.green;
		fr->bgcol[2] = col1.blue;
		fr->bgcol[3] = col1.alpha;

		fr->bgcol2[0] = col2.red;
		fr->bgcol2[1] = col2.green;
		fr->bgcol2[2] = col2.blue;
		fr->bgcol2[3] = col2.alpha;

		fr->grad = grad;

	}
}


static char *get_constant(SCInterpreter *scin, unsigned int constant)
{
	struct sc_state *st = &scin->state[scin->j];
	if ( constant >= NUM_SC_CONSTANTS ) return NULL;
	return st->constants[constant];
}


void sc_interp_set_constant(SCInterpreter *scin, unsigned int constant,
                            const char *val)
{
	struct sc_state *st = &scin->state[scin->j];
	if ( constant >= NUM_SC_CONSTANTS ) return;
	if ( val == NULL ) return;
	st->constants[constant] = strdup(val);
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

		int i;

		if ( st->fontdesc != scin->state[scin->j-1].fontdesc )
		{
			pango_font_description_free(st->fontdesc);
		} /* else the font is the same as the previous one, and we
		   * don't need to free it just yet */

		for ( i=0; i<NUM_SC_CONSTANTS; i++ ) {
			if ( st->constants[i] != scin->state[scin->j-1].constants[i] ) {
				free(st->constants[i]);
			}  /* same logic as above */
		}
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


SCInterpreter *sc_interp_new(PangoContext *pc, PangoLanguage *lang,
                             ImageStore *is, struct frame *top)
{
	SCInterpreter *scin;
	struct sc_state *st;
	int i;

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
	scin->lang = lang;

	/* Initial state */
	st = &scin->state[0];
	st->fr = NULL;
	st->paraspace[0] = 0.0;
	st->paraspace[1] = 0.0;
	st->paraspace[2] = 0.0;
	st->paraspace[3] = 0.0;
	st->fontdesc = NULL;
	st->col[0] = 0.0;
	st->col[1] = 0.0;
	st->col[2] = 0.0;
	st->col[3] = 1.0;
	st->alignment = PANGO_ALIGN_LEFT;
	for ( i=0; i<NUM_SC_CONSTANTS; i++ ) {
		st->constants[i] = NULL;
	}

	/* The "ultimate" default font */
	if ( scin->pc != NULL ) {
		set_frame(scin, top);
		set_font(scin, "Cantarell Regular 14");
		set_colour(scin, "#000000");
	}

	return scin;
}


void sc_interp_destroy(SCInterpreter *scin)
{
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


void update_geom(struct frame *fr)
{
	char geom[256];
	snprintf(geom, 255, "%.1fux%.1fu+%.1f+%.1f",
	         fr->w, fr->h, fr->x, fr->y);

	/* FIXME: What if there are other options? */
	sc_block_set_options(fr->scblocks, strdup(geom));
}


static int calculate_dims(const char *opt, struct frame *parent,
                          double *wp, double *hp, double *xp, double *yp)
{
	LengthUnits h_units, w_units;

	if ( parse_dims(opt, wp, hp, &w_units, &h_units, xp, yp) ) {
		return 1;
	}

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

	return 0;
}


static int parse_frame_option(const char *opt, struct frame *fr,
                              struct frame *parent)
{
	if ( (index(opt, 'x') != NULL) && (index(opt, '+') != NULL)
	  && (index(opt, '+') != rindex(opt, '+')) ) {
		return calculate_dims(opt, parent, &fr->w, &fr->h, &fr->x, &fr->y);
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
		return calculate_dims(opt, NULL, wp, hp, &dum, &dum);
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


static void maybe_recurse_after(SCInterpreter *scin, SCBlock *child,
                                Stylesheet *ss)
{
	if ( child == NULL ) return;

	sc_interp_add_blocks(scin, child, ss);
	sc_interp_restore(scin);
}


static void add_newpara(SCBlock *bl, SCInterpreter *scin)
{
	Paragraph *last_para;
	Paragraph *para;
	struct sc_state *st = &scin->state[scin->j];
	struct frame *fr = sc_interp_get_frame(scin);

	if ( fr->paras == NULL ) return;
	last_para = fr->paras[fr->n_paras-1];

	set_newline_at_end(last_para, bl);

	/* The block after the \newpara will always be the first one of the
	 * next paragraph, by definition, even if it's \f or another \newpara */
	para = create_paragraph(fr, sc_block_next(bl));
	set_para_alignment(para, st->alignment);
	set_para_spacing(para, st->paraspace);
}


/* Add the SCBlock to the text in 'frame', at the end */
static int add_text(struct frame *fr, PangoContext *pc, SCBlock *bl,
                    PangoLanguage *lang, int editable, SCInterpreter *scin,
                    const char *real_text)
{
	const char *text = sc_block_contents(bl);
	PangoFontDescription *fontdesc;
	double *col;
	struct sc_state *st = &scin->state[scin->j];
	Paragraph *para;

	/* Empty block? */
	if ( text == NULL && real_text == NULL ) return 1;

	fontdesc = sc_interp_get_fontdesc(scin);
	col = sc_interp_get_fgcol(scin);

	para = last_para(fr);
	if ( (para == NULL) || (para_type(para) != PARA_TYPE_TEXT) ) {
		/* Last paragraph is not text.
		 *  or: no paragraphs yet.
		 *    Either way: Create the first one */
		para = create_paragraph(fr, bl);
	}

	set_para_alignment(para, st->alignment);
	add_run(para, bl, fontdesc, col, real_text);
	set_para_spacing(para, st->paraspace);

	return 0;
}


static void apply_style(SCInterpreter *scin, Stylesheet *ss, const char *path)
{
	char *result;

	if ( ss == NULL ) return;

	/* Font */
	result = stylesheet_lookup(ss, path, "font");
	if ( result != NULL ) set_font(scin, result);

	/* Foreground colour */
	result = stylesheet_lookup(ss, path, "fgcol");
	if ( result != NULL ) set_colour(scin, result);

	/* Background (vertical gradient) */
	result = stylesheet_lookup(ss, path, "bggradv");
	if ( result != NULL ) set_bggrad(scin, result, GRAD_VERT);

	/* Background (horizontal gradient) */
	result = stylesheet_lookup(ss, path, "bggradh");
	if ( result != NULL ) set_bggrad(scin, result, GRAD_HORIZ);

	/* Background (solid colour) */
	result = stylesheet_lookup(ss, path, "bgcol");
	if ( result != NULL ) set_bgcol(scin, result);

	/* Padding */
	result = stylesheet_lookup(ss, path, "pad");
	if ( result != NULL ) set_padding(sc_interp_get_frame(scin), result);

	/* Paragraph spacing */
	result = stylesheet_lookup(ss, path, "paraspace");
	if ( result != NULL ) set_paraspace(scin, result);

	/* Alignment */
	result = stylesheet_lookup(ss, path, "alignment");
	if ( result != NULL ) {
		if ( strcmp(result, "center") == 0 ) {
			set_alignment(scin, PANGO_ALIGN_CENTER);
		}
		if ( strcmp(result, "left") == 0 ) {
			set_alignment(scin, PANGO_ALIGN_LEFT);
		}
		if ( strcmp(result, "right") == 0 ) {
			set_alignment(scin, PANGO_ALIGN_RIGHT);
		}
	}
}


static void output_frame(SCInterpreter *scin, SCBlock *bl, Stylesheet *ss,
                         const char *stylename)
{
	struct frame *fr;
	SCBlock *child = sc_block_child(bl);
	const char *options = sc_block_options(bl);
	char *result;

	fr = add_subframe(sc_interp_get_frame(scin));
	if ( fr == NULL ) {
		fprintf(stderr, _("Failed to add frame.\n"));
		return;
	}

	fr->scblocks = bl;
	fr->resizable = 1;

	/* Lowest priority: current state of interpreter */
	set_frame_default_style(fr, scin);

	/* Next priority: geometry from stylesheet */
	result = stylesheet_lookup(ss, stylename, "geometry");
	if ( result != NULL ) {
		parse_frame_options(fr, sc_interp_get_frame(scin), result);
	}

	/* Highest priority: parameters to \f (or \slidetitle etc) */
	parse_frame_options(fr, sc_interp_get_frame(scin), options);

	maybe_recurse_before(scin, child);
	set_frame(scin, fr);
	apply_style(scin, ss, stylename);
	maybe_recurse_after(scin, child, ss);
}


static int check_outputs(SCBlock *bl, SCInterpreter *scin, Stylesheet *ss)
{
	const char *name = sc_block_name(bl);
	const char *options = sc_block_options(bl);

	if ( name == NULL ) {
		add_text(sc_interp_get_frame(scin),
		         scin->pc, bl, scin->lang, 1, scin, NULL);

	} else if ( strcmp(name, "image")==0 ) {
		double w, h;
		char *filename;
		if ( parse_image_options(options, sc_interp_get_frame(scin),
		                         &w, &h, &filename) == 0 )
		{
			add_image_para(sc_interp_get_frame(scin), bl,
			               filename, scin->is, w, h, 1);
			free(filename);
		} else {
			fprintf(stderr, _("Invalid image options '%s'\n"),
			        options);
		}

	} else if ( strcmp(name, "f")==0 ) {
		output_frame(scin, bl, ss, "$.slide.frame");

	} else if ( strcmp(name, "slidetitle")==0 ) {
		output_frame(scin, bl, ss, "$.slide.slidetitle");

	} else if ( strcmp(name, "prestitle")==0 ) {
		output_frame(scin, bl, ss, "$.slide.prestitle");

	} else if ( strcmp(name, "author")==0 ) {
		output_frame(scin, bl, ss, "$.slide.author");

	} else if ( strcmp(name, "footer")==0 ) {
		output_frame(scin, bl, ss, "$.slide.footer");

	} else if ( strcmp(name, "newpara")==0 ) {
		add_newpara(bl, scin);

	} else if ( strcmp(name, "slidenumber")==0 ) {
		char *con = get_constant(scin, SCCONST_SLIDENUMBER);
		if ( con != NULL ) {
			add_text(sc_interp_get_frame(scin), scin->pc, bl,
			         scin->lang, 1, scin, con);
		}

	} else {
		return 0;
	}

	return 1;  /* handled */
}


int sc_interp_add_block(SCInterpreter *scin, SCBlock *bl, Stylesheet *ss)
{
	const char *name = sc_block_name(bl);
	const char *options = sc_block_options(bl);
	SCBlock *child = sc_block_child(bl);

	//printf("Running this --------->\n");
	//show_sc_blocks(bl);
	//printf("<------------\n");

	if ( check_callback(scin, bl) ) {
		/* Handled in check_callback, don't do anything else */

	} else if ((sc_interp_get_frame(scin) != NULL)
	  && check_outputs(bl, scin, ss) ) {
		/* Block handled as output thing */

	} else if ( name == NULL ) {
		/* Dummy to ensure name != NULL below */

	} else if ( strcmp(name, "presentation") == 0 ) {
		maybe_recurse_before(scin, child);
		apply_style(scin, ss, "$.narrative");
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "slide") == 0 ) {
		maybe_recurse_before(scin, child);
		apply_style(scin, ss, "$.slide");
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "font") == 0 ) {
		maybe_recurse_before(scin, child);
		set_font(scin, options);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "fontsize") == 0 ) {
		maybe_recurse_before(scin, child);
		set_fontsize(scin, options);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "bold") == 0 ) {
		maybe_recurse_before(scin, child);
		set_bold(scin);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "oblique") == 0 ) {
		maybe_recurse_before(scin, child);
		set_oblique(scin);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "italic") == 0 ) {
		maybe_recurse_before(scin, child);
		set_italic(scin);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "lalign") == 0 ) {
		maybe_recurse_before(scin, child);
		set_alignment(scin, PANGO_ALIGN_LEFT);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "ralign") == 0 ) {
		maybe_recurse_before(scin, child);
		set_alignment(scin, PANGO_ALIGN_RIGHT);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "center") == 0 ) {
		maybe_recurse_before(scin, child);
		set_alignment(scin, PANGO_ALIGN_CENTER);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "fgcol") == 0 ) {
		maybe_recurse_before(scin, child);
		set_colour(scin, options);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "pad") == 0 ) {
		maybe_recurse_before(scin, child);
		set_padding(sc_interp_get_frame(scin), options);
		maybe_recurse_after(scin, child, ss);

	} else if ( strcmp(name, "bgcol") == 0 ) {
		set_bgcol(scin, options);

	} else if ( strcmp(name, "bggradh") == 0 ) {
		set_bggrad(scin, options, GRAD_HORIZ);

	} else if ( strcmp(name, "bggradv") == 0 ) {
		set_bggrad(scin, options, GRAD_VERT);

	} else if ( strcmp(name, "paraspace") == 0 ) {
		maybe_recurse_before(scin, child);
		set_paraspace(scin, options);
		maybe_recurse_after(scin, child, ss);

	} else {

		fprintf(stderr, "Don't know what to do with this:\n");
		show_sc_block(bl, "");

	}

	return 0;
}

