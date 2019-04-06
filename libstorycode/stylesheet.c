/*
 * stylesheet.c
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "stylesheet.h"
#include "storycode.h"

enum style_mask
{
	SM_FRAME_GEOM = 1<<0,
	SM_FONT       = 1<<1,
	SM_FGCOL      = 1<<2,
	SM_BGCOL      = 1<<3,  /* Includes bggrad, bgcol and bgcol2 */
	SM_PARASPACE  = 1<<4,
	SM_PADDING    = 1<<5,
	SM_ALIGNMENT  = 1<<6,
};


struct style
{
	char *name;
	enum style_mask set;

	struct frame_geom geom;
	char *font;
	struct colour fgcol;
	enum gradient bggrad;
	struct colour bgcol;
	struct colour bgcol2;
	struct length paraspace[4];  /* l r t b */
	struct length padding[4];    /* l r t b */
	enum alignment alignment;

	int n_substyles;
	struct style *substyles;
};


struct _stylesheet
{
	struct style top;
};


static void copy_col(struct colour *to, struct colour from)
{
    int i;
    for ( i=0; i<4; i++ ) to->rgba[i] = from.rgba[i];
    to->hexcode = from.hexcode;
}


static void default_style(struct style *s)
{
	s->geom.x.len = 0.0;
	s->geom.x.unit = LENGTH_FRAC;
	s->geom.y.len = 0.0;
	s->geom.y.unit = LENGTH_FRAC;
	s->geom.w.len = 1.0;
	s->geom.w.unit = LENGTH_FRAC;
	s->geom.h.len = 1.0;
	s->geom.h.unit = LENGTH_FRAC;

	s->font = strdup("Sans 12");
	s->alignment = ALIGN_LEFT;

	s->fgcol.rgba[0] = 0.0;
	s->fgcol.rgba[1] = 0.0;
	s->fgcol.rgba[2] = 0.0;
	s->fgcol.rgba[3] = 1.0;
	s->fgcol.hexcode = 1;

	s->bggrad = GRAD_NONE;

	s->bgcol.rgba[0] = 1.0;
	s->bgcol.rgba[1] = 1.0;
	s->bgcol.rgba[2] = 1.0;
	s->bgcol.rgba[3] = 1.0;
	s->bgcol.hexcode = 1;

	s->bgcol2.rgba[0] = 1.0;
	s->bgcol2.rgba[1] = 1.0;
	s->bgcol2.rgba[2] = 1.0;
	s->bgcol2.rgba[3] = 1.0;
	s->bgcol2.hexcode = 1;

	s->paraspace[0].len = 0.0;
	s->paraspace[1].len = 0.0;
	s->paraspace[2].len = 0.0;
	s->paraspace[3].len = 0.0;
	s->paraspace[0].unit = LENGTH_UNIT;
	s->paraspace[1].unit = LENGTH_UNIT;
	s->paraspace[2].unit = LENGTH_UNIT;
	s->paraspace[3].unit = LENGTH_UNIT;

	s->padding[0].len = 0.0;
	s->padding[1].len = 0.0;
	s->padding[2].len = 0.0;
	s->padding[3].len = 0.0;
	s->padding[0].unit = LENGTH_UNIT;
	s->padding[1].unit = LENGTH_UNIT;
	s->padding[2].unit = LENGTH_UNIT;
	s->padding[3].unit = LENGTH_UNIT;
}


void stylesheet_free(Stylesheet *s)
{
	free(s);
}


static int strdotcmp(const char *a, const char *b)
{
	int i = 0;
	do {
		if ( (b[i] != '.') && (a[i] != b[i]) ) return 1;
		i++;
	} while ( (a[i] != '\0') && (b[i] != '\0') && (b[i] != '.') );
	return 0;
}


static struct style *lookup_style(struct style *sty, const char *path)
{
	int i;
	const char *nxt = path;

	if ( path[0] == '\0' ) return sty;

	for ( i=0; i<sty->n_substyles; i++ ) {
		if ( strdotcmp(sty->substyles[i].name, path) == 0 ) {
			sty = &sty->substyles[i];
			break;
		}
	}

	nxt = strchr(nxt, '.');
	if ( nxt == NULL ) return sty;
	return lookup_style(sty, nxt+1);
}


static struct style *create_style(Stylesheet *ss, const char *path, const char *name)
{
	struct style *sty;
	struct style *substy_new;
	struct style *sty_new;

	sty = lookup_style(&ss->top, path);
	substy_new = realloc(sty->substyles, (sty->n_substyles+1)*sizeof(struct style));
	if ( substy_new == NULL ) return NULL;

	sty->substyles = substy_new;
	sty->substyles[sty->n_substyles].n_substyles = 0;
	sty->substyles[sty->n_substyles].substyles = NULL;

	sty_new = &sty->substyles[sty->n_substyles++];

	sty_new->set = 0;
	default_style(sty_new);
	sty_new->name = strdup(name);

	return sty_new;
}


Stylesheet *stylesheet_new()
{
	Stylesheet *ss;
	struct style *sty;

	ss = malloc(sizeof(*ss));
	if ( ss == NULL ) return NULL;

	ss->top.n_substyles = 0;
	ss->top.substyles = NULL;
	ss->top.name = strdup("");
	ss->top.set = 0;
	default_style(&ss->top);

	create_style(ss, "", "NARRATIVE");
	create_style(ss, "NARRATIVE", "BP");
	create_style(ss, "NARRATIVE", "PRESTITLE");
	sty = create_style(ss, "", "SLIDE");
	sty->geom.w.unit = LENGTH_UNIT;
	sty->geom.w.len = 1024.0;
	sty->geom.h.unit = LENGTH_UNIT;
	sty->geom.h.len = 768.0;
	create_style(ss, "SLIDE", "TEXT");
	create_style(ss, "SLIDE", "PRESTITLE");
	create_style(ss, "SLIDE", "SLIDETITLE");

	return ss;
}


static struct style *get_style(Stylesheet *s, enum style_element el)
{
	if ( s == NULL ) return NULL;
	switch ( el ) {
		case STYEL_NARRATIVE : return lookup_style(&s->top, "NARRATIVE");
		case STYEL_NARRATIVE_BP : return lookup_style(&s->top, "NARRATIVE.BP");
		case STYEL_NARRATIVE_PRESTITLE : return lookup_style(&s->top, "NARRATIVE.PRESTITLE");
		case STYEL_SLIDE : return lookup_style(&s->top, "SLIDE");
		case STYEL_SLIDE_TEXT : return lookup_style(&s->top, "SLIDE.TEXT");
		case STYEL_SLIDE_PRESTITLE : return lookup_style(&s->top, "SLIDE.PRESTITLE");
		case STYEL_SLIDE_SLIDETITLE : return lookup_style(&s->top, "SLIDE.SLIDETITLE");
		default : return NULL;
	}
}


int stylesheet_get_slide_default_size(Stylesheet *s, double *w, double *h)
{
	struct style *sty = lookup_style(&s->top, "SLIDE");
	if ( sty == NULL ) return 1;
	assert(sty->geom.w.unit == LENGTH_UNIT);
	assert(sty->geom.h.unit == LENGTH_UNIT);
	*w = sty->geom.w.len;
	*h = sty->geom.h.len;
	return 0;
}


int stylesheet_set_slide_default_size(Stylesheet *s, double w, double h)
{
	struct style *sty = lookup_style(&s->top, "SLIDE");
	if ( sty == NULL ) return 1;
	assert(sty->geom.w.unit == LENGTH_UNIT);
	assert(sty->geom.h.unit == LENGTH_UNIT);
	sty->geom.w.len = w;
	sty->geom.h.len = h;
	return 0;
}


int stylesheet_set_geometry(Stylesheet *s, enum style_element el, struct frame_geom geom)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	sty->geom = geom;
	sty->set |= SM_FRAME_GEOM;
	return 0;
}


int stylesheet_set_font(Stylesheet *s, enum style_element el, char *font)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	if ( sty->font != NULL ) {
		free(sty->font);
	}
	sty->font = font;
	sty->set |= SM_FONT;
	return 0;
}


int stylesheet_set_padding(Stylesheet *s, enum style_element el, struct length padding[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->padding[i] = padding[i];
	}
	sty->set |= SM_PADDING;
	return 0;
}


int stylesheet_set_paraspace(Stylesheet *s, enum style_element el, struct length paraspace[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->paraspace[i] = paraspace[i];
	}
	sty->set |= SM_PARASPACE;
	return 0;
}


int stylesheet_set_fgcol(Stylesheet *s, enum style_element el, struct colour fgcol)
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) {
		sty->fgcol.rgba[i] = fgcol.rgba[i];
	}
	sty->fgcol.hexcode = fgcol.hexcode;
	sty->set |= SM_FGCOL;
	return 0;
}


int stylesheet_set_background(Stylesheet *s, enum style_element el, enum gradient grad,
                              struct colour bgcol, struct colour bgcol2)
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	sty->bggrad = grad;
	for ( i=0; i<4; i++ ) {
		sty->bgcol.rgba[i] = bgcol.rgba[i];
		sty->bgcol2.rgba[i] = bgcol2.rgba[i];
	}
	sty->bgcol.hexcode = bgcol.hexcode;
	sty->bgcol2.hexcode = bgcol2.hexcode;
	sty->set |= SM_BGCOL;
	return 0;
}


int stylesheet_set_alignment(Stylesheet *s, enum style_element el, enum alignment align)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	assert(align != ALIGN_INHERIT);
	sty->alignment = align;
	sty->set |= SM_ALIGNMENT;
	return 0;
}


int stylesheet_get_geometry(Stylesheet *s, enum style_element el, struct frame_geom *geom)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	*geom = sty->geom;
	return 0;
}


const char *stylesheet_get_font(Stylesheet *s, enum style_element el,
                                struct colour *fgcol, enum alignment *alignment)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return NULL;

	*alignment = sty->alignment;
	if ( fgcol != NULL ) {
		copy_col(fgcol, sty->fgcol);
	}
	return sty->font;
}


int stylesheet_get_background(Stylesheet *s, enum style_element el,
                              enum gradient *grad, struct colour *bgcol,
                              struct colour *bgcol2)
{
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;

	copy_col(bgcol, sty->bgcol);
	copy_col(bgcol2, sty->bgcol2);
	*grad = sty->bggrad;
	return 0;
}


int stylesheet_get_padding(Stylesheet *s, enum style_element el,
                           struct length padding[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) padding[i] = sty->padding[i];
	return 0;
}


int stylesheet_get_paraspace(Stylesheet *s, enum style_element el,
                           struct length paraspace[4])
{
	int i;
	struct style *sty = get_style(s, el);
	if ( sty == NULL ) return 1;
	for ( i=0; i<4; i++ ) paraspace[i] = sty->paraspace[i];
	return 0;
}


static void add_text(char **text, size_t *len, size_t *lenmax, const char *prefix,
                     const char *tadd)
{
	size_t taddlen, prefixlen;

	if ( *text == NULL ) return;
	taddlen = strlen(tadd);
	prefixlen = strlen(prefix);

	if ( *len + taddlen + prefixlen + 1 > *lenmax ) {
		*lenmax += taddlen + prefixlen + 32;
		char *text_new = realloc(*text, *lenmax);
		if ( text_new == NULL ) {
			*text = NULL;
			return;
		}
		*text = text_new;
	}

	strcat(*text, prefix);
	strcat(*text, tadd);
	*len += taddlen + prefixlen;
}


static void format_col(char *a, size_t max_len, struct colour col)
{
	if ( !col.hexcode ) {
		snprintf(a, max_len, "%.4g,%.4g,%.4g,%.4g",
		         col.rgba[0], col.rgba[1], col.rgba[2], col.rgba[3]);
	} else {
		snprintf(a, max_len, "#%.2X%.2X%.2X",
		         (unsigned int)(col.rgba[0]*255),
		         (unsigned int)(col.rgba[1]*255),
		         (unsigned int)(col.rgba[2]*255));
	}
}


static void add_style(char **text, size_t *len, size_t *lenmax, const char *prefix,
                      struct style *sty)
{
	char *prefix2;
	int i;

	if ( sty->set & SM_FRAME_GEOM ) {
		char tmp[256];
		snprintf(tmp, 255, "GEOMETRY %.4g%c x %.4g%c +%.4g%c +%.4g%c\n",
		         sty->geom.w.len, unitc(sty->geom.w.unit),
		         sty->geom.h.len, unitc(sty->geom.h.unit),
		         sty->geom.x.len, unitc(sty->geom.x.unit),
		         sty->geom.y.len, unitc(sty->geom.y.unit));
		add_text(text, len, lenmax, prefix, tmp);
	}

	if ( sty->set & SM_FONT ) {
		char tmp[256];
		snprintf(tmp, 255, "FONT %s\n", sty->font);
		add_text(text, len, lenmax, prefix, tmp);
	}

	if ( sty->set & SM_FGCOL ) {
		char tmp[256];
		format_col(tmp, 255, sty->fgcol);
		add_text(text, len, lenmax, prefix, "FGCOL ");
		add_text(text, len, lenmax, "", tmp);
		add_text(text, len, lenmax, "", "\n");
	}

	if ( sty->set & SM_BGCOL ) {
		char tmp[256];
		add_text(text, len, lenmax, prefix, "BGCOL ");
		add_text(text, len, lenmax, "", bgcolc(sty->bggrad));
		format_col(tmp, 255, sty->bgcol);
		add_text(text, len, lenmax, "", tmp);
		if ( sty->bggrad != GRAD_NONE ) {
			format_col(tmp, 255, sty->bgcol2);
			add_text(text, len, lenmax, " ", tmp);
		}
		add_text(text, len, lenmax, "", "\n");
	}

	if ( sty->set & SM_PARASPACE ) {
		char tmp[256];
		snprintf(tmp, 255, "PARASPACE %.4g%c,%.4g%c,%.4g%c,%.4g%c\n",
		         sty->paraspace[0].len, unitc(sty->paraspace[0].unit),
		         sty->paraspace[1].len, unitc(sty->paraspace[1].unit),
		         sty->paraspace[2].len, unitc(sty->paraspace[2].unit),
		         sty->paraspace[3].len, unitc(sty->paraspace[3].unit));
		add_text(text, len, lenmax, prefix, tmp);
	}

	if ( sty->set & SM_PADDING ) {
		char tmp[256];
		snprintf(tmp, 255, "PAD %.4g%c,%.4g%c,%.4g%c,%.4g%c\n",
		         sty->padding[0].len, unitc(sty->padding[0].unit),
		         sty->padding[1].len, unitc(sty->padding[1].unit),
		         sty->padding[2].len, unitc(sty->padding[2].unit),
		         sty->padding[3].len, unitc(sty->padding[3].unit));
		add_text(text, len, lenmax, prefix, tmp);
	}

	if ( sty->set & SM_ALIGNMENT ) {
		char tmp[256];
		snprintf(tmp, 255, "ALIGN %s\n", alignc(sty->alignment));
		add_text(text, len, lenmax, prefix, tmp);
	}

	prefix2 = malloc(strlen(prefix)+3);
	strcpy(prefix2, prefix);
	strcat(prefix2, "  ");
	for ( i=0; i<sty->n_substyles; i++ ) {
		add_text(text, len, lenmax, prefix, sty->substyles[i].name);
		add_text(text, len, lenmax, "", " {\n");
		add_style(text, len, lenmax, prefix2, &sty->substyles[i]);
		add_text(text, len, lenmax, prefix, "}\n");
	}
	free(prefix2);

	if ( strcmp(sty->name, "SLIDE") == 0 ) {
		char tmp[256];
		snprintf(tmp, 255, "SIZE %.4gu x %4gu\n",
		         sty->geom.w.len, sty->geom.h.len);
		add_text(text, len, lenmax, prefix, tmp);
	}
}


char *stylesheet_serialise(Stylesheet *s)
{
	size_t len = 0;      /* Current length of "text", not including \0 */
	size_t lenmax = 32;  /* Current amount of memory allocated to "text" */
	char *text;

	text = malloc(lenmax*sizeof(char));
	if ( text == NULL ) return NULL;
	text[0] = '\0';

	add_text(&text, &len, &lenmax, "", "STYLES {\n");
	add_style(&text, &len, &lenmax, "  ", &s->top);
	add_text(&text, &len, &lenmax, "", "}\n\n");

	return text;
}
