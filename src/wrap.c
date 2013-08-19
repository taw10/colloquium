/*
 * wrap.c
 *
 * Text wrapping, hyphenation, justification and shaping
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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
#include <math.h>
#include <gdk/gdk.h>

#include "storycode.h"
#include "wrap.h"
#include "frame.h"
#include "stylesheet.h"


static void alloc_lines(struct frame *fr)
{
	struct wrap_line *lines_new;

	lines_new = realloc(fr->lines, fr->max_lines * sizeof(struct wrap_line));
	if ( lines_new == NULL ) {
		fprintf(stderr, "Couldn't allocate memory for lines!\n");
		return;
	}

	fr->lines = lines_new;
}


static void alloc_boxes(struct wrap_line *l)
{
	struct wrap_box *boxes_new;

	boxes_new = realloc(l->boxes, l->max_boxes * sizeof(struct wrap_box));
	if ( boxes_new == NULL ) {
		fprintf(stderr, "Couldn't allocate memory for boxes!\n");
		return;
	}

	l->boxes = boxes_new;
}


static void initialise_line(struct wrap_line *l)
{
	l->n_boxes = 0;
	l->max_boxes = 32;
	l->boxes = NULL;
	l->width = 0;
	l->height = 0;
	l->ascent = 0;
	l->last_line = 0;
	l->overfull = 0;
	alloc_boxes(l);
}



static struct wrap_line *get_cursor_line(struct frame *fr, size_t pos,
					 double *yposd)
{
	int line = 0;
	int i;
	int found = 0;

	*yposd = 0;

	for ( i=0; i<fr->n_lines; i++ ) {
		if ( fr->lines[i].sc_offset > pos ) {
			line = i-1;
			found = 1;
			break;
		}
	}
	if ( !found ) {
		/* Cursor is on the last line */
		line = fr->n_lines-1;
	}
	if ( line < 0 ) {
		printf("Couldn't find cursor.\n");
		return NULL;
	}
	for ( i=0; i<line; i++ ) {
		*yposd += fr->lines[i].height;
	}

	*yposd /= PANGO_SCALE;
	*yposd += fr->lop.pad_t;

	return &fr->lines[line];
}


void get_cursor_pos(struct frame *fr, size_t pos,
                    double *xposd, double *yposd, double *line_height)
{
	signed int i;
	struct wrap_line *l;
	struct wrap_box *b;
	int p;
	int box;

	*xposd = 0.0;
	*yposd = 0.0;
	*line_height = 20.0;

	if ( fr->n_lines == 0 ) return;

	l = get_cursor_line(fr, pos, yposd);
	if ( l == NULL ) return;

	*line_height = pango_units_to_double(l->height);

	for ( box=1; box<l->n_boxes; box++ ) {
		/* Was the cursor in the previous box? */
		if ( l->boxes[box].type == WRAP_BOX_SENTINEL ) break;
		if ( l->boxes[box].sc_offset > pos ) break;
	}
	box--;

	*xposd = fr->lop.pad_l;
	for ( i=0; i<box; i++ ) {
		*xposd += pango_units_to_double(l->boxes[i].width);
		*xposd += pango_units_to_double(l->boxes[i].sp);
	}

	b = &l->boxes[box];
	switch ( b->type ) {

		case WRAP_BOX_PANGO :
		pango_glyph_string_index_to_x(b->glyphs, b->text,
		                              strlen(b->text),
			                      &b->item->analysis,
			                      pos - b->sc_offset,
			                      FALSE, &p);
		*xposd += pango_units_to_double(p);
		break;

		case WRAP_BOX_IMAGE :
		break;

	}
}


static struct wrap_line *find_cursor_line(struct frame *fr, double yposd,
					  int *end)
{
	int i;
	double y = fr->lop.pad_t;

	*end = 0;

	for ( i=0; i<fr->n_lines; i++ ) {
		double height = pango_units_to_double(fr->lines[i].height);
		if ( yposd < y + height ) {
			return &fr->lines[i];
		}
		y += height;
	}

	*end = 1;
	return &fr->lines[fr->n_lines-1];
}


static struct wrap_box *find_cursor_box(struct frame *fr, struct wrap_line *l,
					double xposd, double *x_pos, int *end)
{
	int i;
	double x = fr->lop.pad_l;

	*end = 0;

	for ( i=0; i<l->n_boxes; i++ ) {
		double width = pango_units_to_double(l->boxes[i].width);
		width += pango_units_to_double(l->boxes[i].sp);
		if ( xposd < x + width ) {
			*x_pos = xposd - x;
			return &l->boxes[i];
		}
		x += width;
	}

	*end = 1;
	*x_pos = xposd - x;
	return &l->boxes[l->n_boxes-1];
}


size_t find_cursor_pos(struct frame *fr, double xposd, double yposd)
{
	struct wrap_line *l;
	struct wrap_box *b;
	int end;
	double x_pos = 0.0;
	int idx, trail;
	int x_pos_i;

	l = find_cursor_line(fr, yposd, &end);

	if ( end ) {
		b = &l->boxes[l->n_boxes - 1];
	} else {
		b = find_cursor_box(fr, l, xposd, &x_pos, &end);
	}

	switch ( b->type ) {

		case WRAP_BOX_NOTHING :
		fprintf(stderr, "Clicked a nothing box!\n");
		abort();

		case WRAP_BOX_PANGO :
		x_pos_i = pango_units_from_double(x_pos);
		pango_glyph_string_x_to_index(b->glyphs, b->text,
		                              strlen(b->text),
		                              &b->item->analysis,
		                              x_pos_i, &idx, &trail);
		/* FIXME: Assumes 1 byte char */
		return b->sc_offset + idx + trail;

		case WRAP_BOX_SENTINEL :
		return l->boxes[l->n_boxes-2].sc_offset;

	}

	return b->sc_offset;
}


static void shape_and_measure(gpointer data, gpointer user_data)
{
	struct wrap_box *box = user_data;
	PangoItem *item = data;
	PangoRectangle rect;

	/* FIXME: Don't assume only one run per wrap box */
	box->glyphs = pango_glyph_string_new();
	box->item = item;

	pango_shape(box->text+item->offset, item->length, &item->analysis,
	            box->glyphs);

	pango_glyph_string_extents(box->glyphs, box->font, NULL, &rect);

	box->width += rect.width;
	if ( rect.height > box->height ) {
		box->height = rect.height;
	}
	if ( PANGO_ASCENT(rect) > box->ascent ) {
		box->ascent = PANGO_ASCENT(rect);
	}
}


static void calc_line_geometry(struct wrap_line *line)
{
	int i;

	line->width = 0;
	line->ascent = 0;
	line->height = 0;

	for ( i=0; i<line->n_boxes; i++ ) {
		struct wrap_box *box = &line->boxes[i];
		line->width += box->width;
		if ( box->space == WRAP_SPACE_EOP ) box->sp = 0.0;
		line->width += box->sp;
		if ( box->height > line->height ) line->height = box->height;
		if ( box->ascent > line->ascent ) line->ascent = box->ascent;
	}
}


struct sc_font
{
	PangoFontDescription *fontdesc;
	PangoFont *font;
	double col[4];
	int ascent;
	int height;
	int free_font_on_pop;
};


/* Add "text", followed by a space of type "space", to "line" */
static int add_wrap_box(struct wrap_line *line, char *text, size_t offset,
                        enum wrap_box_space space, PangoContext *pc,
                        struct sc_font *font)
{
	GList *pango_items;
	struct wrap_box *box;
	PangoAttrList *attrs;
	PangoAttribute *attr;

	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return 1;
	}

	box = &line->boxes[line->n_boxes];
	box->sc_offset = offset;
	box->type = WRAP_BOX_PANGO;
	box->text = text;
	box->space = space;
	box->font = font->font;
	box->width = 0;
	box->ascent = font->ascent;
	box->height = font->height;
	box->col[0] = font->col[0];  /* Red */
	box->col[1] = font->col[1];  /* Green */
	box->col[2] = font->col[2];  /* Blue */
	box->col[3] = font->col[3];  /* Alpha */
	line->n_boxes++;

	if ( strlen(text) == 0 ) {
		box->type = WRAP_BOX_NOTHING;
		return 0;
	}

	attrs = pango_attr_list_new();
	attr = pango_attr_font_desc_new(font->fontdesc);
	pango_attr_list_insert_before(attrs, attr);
	pango_items = pango_itemize(pc, text, 0, strlen(text), attrs, NULL);
	g_list_foreach(pango_items, shape_and_measure, box);
	g_list_free(pango_items);
	pango_attr_list_unref(attrs);

	return 0;
}


static void add_image_box(struct wrap_line *line, const char *filename,
                          size_t offset, int w, int h)
{
	struct wrap_box *box;

	box = &line->boxes[line->n_boxes];
	box->sc_offset = offset;
	box->type = WRAP_BOX_IMAGE;
	box->text = NULL;
	box->space = WRAP_SPACE_NONE;
	box->width = pango_units_from_double(w);
	box->ascent = pango_units_from_double(h);
	box->height = pango_units_from_double(h);
	box->filename = strdup(filename);
	line->n_boxes++;
}


static int split_words(struct wrap_line *boxes, PangoContext *pc, char *sc,
                       size_t sc_offset, PangoLanguage *lang,
                       struct sc_font *font)
{
	PangoLogAttr *log_attrs;
	size_t len;
	size_t i, start;

	/* Empty block? */
	if ( sc == NULL ) return 1;

	len = strlen(sc);
	if ( len == 0 ) return 1;

	log_attrs = malloc((len+1)*sizeof(PangoLogAttr));
	if ( log_attrs == NULL ) return 1;

	/* Create glyph string */
	pango_get_log_attrs(sc, len, -1, lang, log_attrs, len+1);

	start = 0;
	for ( i=0; i<len; i++ ) {

		if ( log_attrs[i].is_line_break ) {

			char *word;
			enum wrap_box_space type;
			size_t len;

			/* Stuff up to (but not including) sc[i] forms a
			 * wrap box */
			len = i-start;
			if ( log_attrs[i].is_mandatory_break ) {
				type = WRAP_SPACE_EOP;
				if ( (i>0) && (sc[i-1] == '\n') ) len--;
				if ( (i>0) && (sc[i-1] == '\r') ) len--;
			} else if ( (i>0)
			         && log_attrs[i-1].is_expandable_space ) {
				type = WRAP_SPACE_INTERWORD;
				len--;  /* Not interested in spaces */
			} else {
				type = WRAP_SPACE_NONE;
			}

			word = strndup(sc+start, len);
			if ( word == NULL ) {
				fprintf(stderr, "strndup() failed.\n");
				free(log_attrs);
				return 1;
			}

			if ( add_wrap_box(boxes, word, start+sc_offset, type,
			                  pc, font) ) {
				fprintf(stderr, "Failed to add wrap box.\n");
			}
			start = i;

		}

	}
	if ( i > start ) {

		char *word;
		word = strndup(sc+start, i-start);
		if ( word == NULL ) {
			fprintf(stderr, "strndup() failed.\n");
			free(log_attrs);
			return 1;
		}

		if ( (word[i-start-1] == '\n')  ) {

			/* There is a newline at the end of the SC */
			char *word2;
			word2 = strndup(word, i-start-1);
			add_wrap_box(boxes, word2, start+sc_offset,
			             WRAP_SPACE_EOP, pc, font);
			add_wrap_box(boxes, strdup(""), i+sc_offset,
			             WRAP_SPACE_NONE, pc, font);

		} else {

			add_wrap_box(boxes, word, start+sc_offset,
			             WRAP_SPACE_NONE, pc, font);

		}

	}

	free(log_attrs);
	return 0;
}


struct sc_font_stack
{
	struct sc_font *stack;
	int n_fonts;
	int max_fonts;
};


static void set_font(struct sc_font_stack *stack, const char *font_name,
                     PangoContext *pc)
{
	PangoFontMetrics *metrics;
	struct sc_font *scf;

	scf = &stack->stack[stack->n_fonts-1];

	scf->fontdesc = pango_font_description_from_string(font_name);
	if ( scf->fontdesc == NULL ) {
		fprintf(stderr, "Couldn't describe font.\n");
		return;
	}
	scf->font = pango_font_map_load_font(pango_context_get_font_map(pc),
	                                     pc, scf->fontdesc);
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
static void set_colour(struct sc_font_stack *stack, const char *colour)
{
	GdkRGBA col;
	struct sc_font *scf = &stack->stack[stack->n_fonts-1];

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


static void copy_top_font(struct sc_font_stack *stack)
{
	if ( stack->n_fonts == stack->max_fonts ) {

		struct sc_font *stack_new;

		stack_new = realloc(stack->stack, sizeof(struct sc_font)
		                     * ((stack->max_fonts)+8));
		if ( stack_new == NULL ) {
			fprintf(stderr, "Failed to push font or colour.\n");
			return;
		}

		stack->stack = stack_new;
		stack->max_fonts += 8;

	}

	/* When n_fonts=0, we leave the first font uninitialised.  This allows
	 * the stack to be "bootstrapped", but requires the first caller to do
	 * set_font and set_colour straight away. */
	if ( stack->n_fonts > 0 ) {
		stack->stack[stack->n_fonts] = stack->stack[stack->n_fonts-1];
	}

	/* This is a copy, so don't free it later */
	stack->stack[stack->n_fonts].free_font_on_pop = 0;

	stack->n_fonts++;
}


static void push_font(struct sc_font_stack *stack, const char *font_name,
                      PangoContext *pc)
{
	copy_top_font(stack);
	set_font(stack, font_name, pc);
}


static void push_colour(struct sc_font_stack *stack, const char *colour)
{
	copy_top_font(stack);
	set_colour(stack, colour);
}


static void pop_font_or_colour(struct sc_font_stack *stack)
{
	struct sc_font *scf = &stack->stack[stack->n_fonts-1];

	if ( scf->free_font_on_pop ) {
		pango_font_description_free(scf->fontdesc);
	}

	stack->n_fonts--;
}


static int get_size(const char *a, int *wp, int *hp)
{
	char *x;
	char *ws;
	char *hs;

	x = index(a, 'x');
	if ( x == NULL ) goto invalid;

	if ( rindex(a, 'x') != x ) goto invalid;

	ws = strndup(a, x-a);
	hs = strdup(x+1);

	*wp = strtoul(ws, NULL, 10);
	*hp = strtoul(hs, NULL, 10);

	free(ws);
	free(hs);

	return 0;

invalid:
	fprintf(stderr, "Invalid dimensions '%s'\n", a);
	return 1;
}


static void run_sc(const char *sc, struct sc_font_stack *fonts,
                   PangoContext *pc, struct wrap_line *boxes,
                   PangoLanguage *lang, size_t g_offset)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;

	bl = sc_find_blocks(sc, NULL);

	if ( bl == NULL ) {
		printf("Failed to find blocks.\n");
		return;
	}

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		size_t offset = b->offset + g_offset;

		if ( b->name == NULL ) {
			split_words(boxes, pc, b->contents, offset,
			            lang, &fonts->stack[fonts->n_fonts-1]);

		} else if ( (strcmp(b->name, "font")==0)
		         && (b->contents == NULL) ) {
			set_font(fonts, b->options, pc);

		} else if ( (strcmp(b->name, "font")==0)
		         && (b->contents != NULL) ) {
			push_font(fonts, b->options, pc);
			run_sc(b->contents, fonts, pc, boxes, lang, offset);
			pop_font_or_colour(fonts);

		} else if ( (strcmp(b->name, "fgcol")==0)
		         && (b->contents == NULL) ) {
			set_colour(fonts, b->options);

		} else if ( (strcmp(b->name, "fgcol")==0)
		         && (b->contents != NULL) ) {
			push_colour(fonts, b->options);
			run_sc(b->contents, fonts, pc, boxes, lang, offset);
			pop_font_or_colour(fonts);

		} else if ( (strcmp(b->name, "image")==0)
		         && (b->contents != NULL) && (b->options != NULL) ) {
			int w, h;
			if ( get_size(b->options, &w, &h) == 0 ) {
				add_image_box(boxes, b->contents, offset, w, h);
			}
		}

	}
	sc_block_list_free(bl);
}


static struct wrap_line *sc_to_wrap_boxes(const char *sc, const char *prefix,
                                          PangoContext *pc)
{
	struct wrap_line *boxes;
	PangoLanguage *lang;
	struct sc_font_stack fonts;

	boxes = malloc(sizeof(struct wrap_line));
	if ( boxes == NULL ) {
		fprintf(stderr, "Failed to allocate boxes.\n");
		return NULL;
	}
	initialise_line(boxes);

	fonts.stack = NULL;
	fonts.n_fonts = 0;
	fonts.max_fonts = 0;

	/* FIXME: Determine proper language (somehow...) */
	lang = pango_language_from_string("en_GB");

	/* The "ultimate" default font */
	push_font(&fonts, "Sans 12", pc);
	set_colour(&fonts, "#000000");

	if ( prefix != NULL ) {
		run_sc(prefix, &fonts, pc, boxes, lang, 0);
	}
	run_sc(sc, &fonts, pc, boxes, lang, 0);

	/* Empty the stack */
	while ( fonts.n_fonts > 0 ) {
		pop_font_or_colour(&fonts);
	}

	return boxes;
}


/* Normal width of space */
static double sp_x(enum wrap_box_space s)
{
	switch ( s ) {

		case WRAP_SPACE_INTERWORD :
		return 10.0*PANGO_SCALE;

		case WRAP_SPACE_EOP :
		return 0.0;

		default:
		case WRAP_SPACE_NONE :
		return 0.0;

	}
}


/* Stretchability of space */
static double sp_y(enum wrap_box_space s)
{
	switch ( s ) {

		case WRAP_SPACE_INTERWORD :
		return 10.0*PANGO_SCALE;

		case WRAP_SPACE_EOP :
		return INFINITY;

		default:
		case WRAP_SPACE_NONE :
		return 0.0;

	}
}


/* Shrinkability of space */
static double sp_z(enum wrap_box_space s)
{
	switch ( s ) {

		case WRAP_SPACE_INTERWORD :
		return 7.0*PANGO_SCALE;

		case WRAP_SPACE_EOP :
		return 0.0;

		default:
		case WRAP_SPACE_NONE :
		return 0.0;

	}
}


/* Minimum width of space */
static double sp_zp(enum wrap_box_space s)
{
	return sp_x(s) - sp_z(s);
}


/* Maximum width of space */
static double sp_yp(enum wrap_box_space s, double rho)
{
	return sp_x(s) + rho*sp_y(s);
}


static void consider_break(double sigma_prime, double sigma_prime_max,
                           double sigma_prime_min, double line_length,
                           double *s, int j, double *dprime, int *jprime,
                           double rho)
{
	double r;
	double d;

	if ( sigma_prime < line_length ) {
		r = rho*(line_length - sigma_prime)
		       / (sigma_prime_max - sigma_prime);
	} else if ( sigma_prime > line_length ) {
		r = rho*(line_length - sigma_prime)
		       / (sigma_prime - sigma_prime_min);
	} else {
		r = 0.0;
	}

	d = s[j] + pow(1.0 + 100.0 * pow(abs(r),3.0), 2.0);
	if ( d < *dprime ) {
		*dprime = d;
		*jprime = j;
	}
}


static double width(struct wrap_line *boxes, int i)
{
	/* Indices in Knuth paper go from 1...n.  Indices in array go
	 * from 0...n-1 */
	return boxes->boxes[i-1].width;
}


static enum wrap_box_space space(struct wrap_line *boxes, int i)
{
	/* Indices in Knuth paper go from 1...n.  Indices in array go
	 * from 0...n-1 */
	return boxes->boxes[i-1].space;
}


static void distribute_spaces(struct wrap_line *line, double l, double rho)
{
	int i;
	double L, Y, Z, r;
	int overfull = 0;
	int underfull = 0;

	l = pango_units_from_double(l);

	L = 0.0;  Y = 0.0;  Z = 0.0;
	for ( i=0; i<line->n_boxes-1; i++ ) {
		L += line->boxes[i].width;
		L += sp_x(line->boxes[i].space);
		Y += sp_y(line->boxes[i].space);
		Z += sp_z(line->boxes[i].space);
	}
	L += line->boxes[line->n_boxes-1].width;

	if ( L < l ) {
		r = (l - L)/Y;
	} else if ( L > l ) {
		r = (l - L)/Z;
	} else {
		r = 0.0;
	}

	if ( r >= 0.0 ) {
		for ( i=0; i<line->n_boxes-1; i++ ) {
			line->boxes[i].sp = sp_x(line->boxes[i].space);
			line->boxes[i].sp += r*sp_y(line->boxes[i].space);
		}
	} else {
		for ( i=0; i<line->n_boxes-1; i++ ) {
			line->boxes[i].sp = sp_x(line->boxes[i].space);
			line->boxes[i].sp += r*sp_z(line->boxes[i].space);
		}
	}

	line->boxes[line->n_boxes-1].sp = 0.0;
	line->overfull = overfull;
	line->underfull = underfull;
}



static void output_line(int q, int s, struct frame *fr, struct wrap_line *boxes)
{
	struct wrap_line *l;
	int j;

	l = &fr->lines[fr->n_lines];
	fr->n_lines++;
	initialise_line(l);

	l->max_boxes = s-q;
	alloc_boxes(l);
	l->sc_offset = boxes->boxes[q].sc_offset;
	for ( j=q; j<s; j++ ) {
		l->boxes[l->n_boxes++] = boxes->boxes[j];
	}
}


static void output(int a, int i, int *p, struct frame *fr,
                   struct wrap_line *boxes)
{
	int q = i;
	int r;
	int s = 0;

	if ( fr->n_lines + (i-a) + 1 > fr->max_lines ) {
		fr->max_lines += 32;
		alloc_lines(fr);
		if ( fr->n_lines == fr->max_lines ) return;
	}

	while ( q != a ) {
		r = p[q];
		p[q] = s;
		s = q;
		q = r;
	}

	while ( q != i ) {

		output_line(q, s, fr, boxes);

		q = s;
		s = p[q];

	}

}


/* This is the "suboptimal fit" algorithm from Knuth and Plass, Software -
 * Practice and Experience 11 (1981) p1119-1184.  Despite the name, it's
 * supposed to work as well as the full TeX algorithm in almost all of the cases
 * that we care about here. */
static void knuth_suboptimal_fit(struct wrap_line *boxes, double line_length,
                                 struct frame *fr, double rho)
{
	int a = 0;
	int *p;
	double *s;
	struct wrap_box *box;
	int j;
	double sigma_prime, sigma_max_prime, sigma_min_prime;
	double dprime;
	int n;
	int reject;

	n = boxes->n_boxes;

	/* Set the space for the last box to be "end of paragraph" */
	boxes->boxes[boxes->n_boxes-1].space = WRAP_SPACE_EOP;

	/* Add empty zero-width box at end */
	if ( boxes->n_boxes == boxes->max_boxes ) {
		boxes->max_boxes += 32;
		alloc_boxes(boxes);
		if ( boxes->n_boxes == boxes->max_boxes ) return;
	}
	box = &boxes->boxes[boxes->n_boxes];
	box->type = WRAP_BOX_SENTINEL;
	box->text = NULL;
	box->space = WRAP_SPACE_NONE;
	box->font = NULL;
	box->width = 0;
	box->ascent = 0;
	box->height = 0;
	boxes->n_boxes++;

	line_length *= PANGO_SCALE;

	reject = 0;
	for ( j=0; j<boxes->n_boxes; j++ ) {
		if ( boxes->boxes[j].width > line_length ) {
			fprintf(stderr, "ERROR: Box %i too long (%i %f)\n", j,
			                boxes->boxes[j].width, line_length);
			reject = 1;
		}
	}
	if ( reject ) return;

	p = malloc((n+2) * sizeof(int));  /* p[0]..p[n+1] inclusive */
	if ( p == NULL ) {
		fprintf(stderr, "Failed to allocate p_k\n");
		return;
	}

	s = malloc((n+2) * sizeof(double));  /* s[0]..s[n+1] inclusive */
	if ( s == NULL ) {
		fprintf(stderr, "Failed to allocate s_k\n");
		return;
	}

	do {

		int i = a;
		int k = i+1;
		double sigma = width(boxes, k);
		double sigma_min = sigma;
		double sigma_max = sigma;
		int m = 1;
		int jprime = 999;

		s[i] = 0;

		do {

			while ( sigma_min > line_length ) {

				/* Begin <advance i by 1> */
				if ( s[i] < INFINITY ) m--;
				i++;
				sigma -= width(boxes, i);
				sigma -= sp_z(space(boxes, i));

				sigma_max -= width(boxes, i);
				sigma_max -= sp_yp(space(boxes, i), rho);

				sigma_min -= width(boxes, i);
				sigma_min -= sp_zp(space(boxes, i));
				/* End */

			}

			/* Begin <examine all feasible lines ending at k> */
			j = i;
			sigma_prime = sigma;
			sigma_max_prime = sigma_max;
			sigma_min_prime = sigma_min;
			dprime = INFINITY;
			while ( sigma_max_prime >= line_length ) {
				if ( s[j] < INFINITY ) {
					consider_break(sigma_prime,
						       sigma_max_prime,
						       sigma_min_prime,
						       line_length, s, j,
						       &dprime, &jprime,
						       rho);
				}
				j++;
				sigma_prime -= width(boxes, j);
				sigma_prime -= sp_x(space(boxes, j));

				sigma_max_prime -= width(boxes, j);
				sigma_max_prime -= sp_yp(space(boxes, j),
					                 rho);

				sigma_min_prime -= width(boxes, j);
				sigma_min_prime -= sp_zp(space(boxes, j));
			}
			/* End */

			s[k] = dprime;
			if ( dprime < INFINITY ) {
				m++;
				p[k] = jprime;
			}
			if ( (m == 0) || (k > n) ) break;
			sigma += width(boxes, k+1);
			sigma += sp_x(space(boxes, k));

			sigma_max += width(boxes, k+1);
			sigma_max += sp_yp(space(boxes, k), rho);

			sigma_min += width(boxes, k+1);
			sigma_min += sp_zp(space(boxes, k));
			k++;

		} while ( 1 );

		if ( k > n ) {
			output(a, n+1, p, fr, boxes);
			break;
		} else {

			/* Begin <try hyphenation> */
			do {

				sigma += width(boxes, i);
				sigma += sp_x(space(boxes, i));

				sigma_max += width(boxes, i);
				sigma_max += sp_yp(space(boxes, i), rho);

				sigma_min += width(boxes, i);
				sigma_min += sp_zp(space(boxes, i));
				i--;

			} while ( s[i] >= INFINITY );
			output(a, i, p, fr, boxes);

			/* Begin <split box k at the best place> */
			jprime = 0;
			dprime = INFINITY;
			/* Test hyphenation points here */
			/* End */

			/* Begin <output and adjust w_k> */
			output_line(i, k-1, fr, boxes);
			/* End */

			/* End */

			a = k-1;

		}

	} while ( 1 );

	fr->lines[fr->n_lines-1].last_line = 1;

	free(p);
	free(s);
}


void wrap_line_free(struct wrap_line *l)
{
	int i;
	for ( i=0; i<l->n_boxes; i++ ) {

		switch ( l->boxes[i].type ) {

			case WRAP_BOX_PANGO :
			pango_glyph_string_free(l->boxes[i].glyphs);
			pango_item_free(l->boxes[i].item);
			free(l->boxes[i].text);
			break;

			case WRAP_BOX_IMAGE :
			break;

			case WRAP_BOX_NOTHING :
			case WRAP_BOX_SENTINEL :
			break;

		}

	}

	free(l->boxes);
}


static struct wrap_line *split_paragraph(struct wrap_line *boxes, int *n)
{
	int i;
	int start = *n;
	int end;

	if ( start >= boxes->n_boxes ) return NULL;

	for ( i=start; i<boxes->n_boxes; i++ ) {
		if ( boxes->boxes[i].space == WRAP_SPACE_EOP ) {
			break;
		}
	}
	end = i + 1;
	*n = end;
	if ( i == boxes->n_boxes ) end--;

	if ( end-start > 0 ) {
		struct wrap_line *para;
		para = malloc(sizeof(struct wrap_line));
		para->boxes = NULL;
		para->max_boxes = end-start;
		para->n_boxes = end-start;
		alloc_boxes(para);
		for ( i=start; i<end; i++ ) {
			para->boxes[i-start] = boxes->boxes[i];
		}
		return para;
	}

	return NULL;
}


void show_boxes(struct wrap_line *boxes)
{
	int i;

	if ( boxes == NULL ) {
		printf("Empty line.\n");
		return;
	}

	for ( i=0; i<boxes->n_boxes; i++ ) {
		printf("%3i", i);
		printf(" '%s'", boxes->boxes[i].text);
		printf(" t=%i s=%i %i %5.2f\n",
		       boxes->boxes[i].type, boxes->boxes[i].space,
		       boxes->boxes[i].width, boxes->boxes[i].sp);
	}
}


/* Wrap the StoryCode inside "fr->sc" so that it fits within width "fr->w",
 * and generate fr->lines */
int wrap_contents(struct frame *fr, PangoContext *pc)
{
	struct wrap_line *boxes;
	struct wrap_line *para;
	int i;
	const double rho = 2.0;
	const double wrap_w = fr->w - fr->lop.pad_l - fr->lop.pad_r;
	char *prologue;

	if ( fr->style != NULL ) {
		prologue = fr->style->sc_prologue;
	} else {
		prologue = NULL;
	}

	/* Turn the StoryCode into wrap boxes, all on one line */
	boxes = sc_to_wrap_boxes(fr->sc, prologue, pc);
	if ( boxes == NULL ) {
		fprintf(stderr, "Failed to create wrap boxes.\n");
		return 1;
	}

	/* Clear lines */
	fr->n_lines = 0;
	fr->max_lines = 32;
	fr->lines = NULL;
	alloc_lines(fr);

	/* Split text into paragraphs */
	i = 0;
	do {

		para = split_paragraph(boxes, &i);

		/* Split paragraphs into lines */
		if ( para != NULL ) {

			knuth_suboptimal_fit(para, wrap_w, fr, rho);

			free(para->boxes);
			free(para);

		}

	} while ( para != NULL );

	free(boxes->boxes);
	free(boxes);

	for ( i=0; i<fr->n_lines; i++ ) {
		distribute_spaces(&fr->lines[i], wrap_w, rho);
		calc_line_geometry(&fr->lines[i]);
	}

	return 0;
}
