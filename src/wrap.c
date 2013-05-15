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


void get_cursor_pos(struct frame *fr, size_t pos,
                    double *xposd, double *yposd, double *line_height)
{
	int line, box;
	signed int i;
	struct wrap_line *l;
	struct wrap_box *b;
	int p;
	int found = 0;

	*xposd = 0;
	*yposd = 0;

	if ( fr->n_lines == 0 ) return;

	line = 0;
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
	assert(line >= 0);
	for ( i=0; i<line; i++ ) {
		*yposd += fr->lines[i].height;
	}

	*line_height = pango_units_to_double(fr->lines[line].height);
	*yposd /= PANGO_SCALE;
	*yposd += fr->lop.pad_t;

	l = &fr->lines[line];
	box = 0;
	for ( i=0; i<l->n_boxes-1; i++ ) {
		box = i;
		if ( l->boxes[i+1].type == WRAP_BOX_SENTINEL ) break;
		if ( l->boxes[i+1].sc_offset > pos ) break;
		*xposd += l->boxes[i].width;
		if ( i < l->n_boxes-2 ) {
			*xposd += l->boxes[i].sp;
		}
	}

	b = &l->boxes[box];
	if ( b->type == WRAP_BOX_PANGO ) {
		pango_glyph_string_index_to_x(b->glyphs, b->text,
		                              strlen(b->text),
			                      &b->item->analysis,
			                      pos - b->sc_offset,
			                      FALSE, &p);
		//printf("offset %i in '%s' -> %i\n",
		//       (int)pos-(int)b->sc_offset, b->text, p);

		*xposd += p;
		*xposd /= PANGO_SCALE;
		*xposd += fr->lop.pad_l;
		//printf("%i  ->  line %i, box %i  -> %f, %f\n",
		//       (int)pos, line, box, *xposd, *yposd);

	} else {
	}
}


#if 0
size_t find_cursor_pos(struct frame *fr, double xposd, double yposd)
{
	int line, box;
	signed int i;
	struct wrap_line *l;
	struct wrap_box *b;
	int p;
	int found = 0;

	*xposd = 0;
	*yposd = 0;

	line = 0;
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
	assert(line >= 0);
	for ( i=0; i<line; i++ ) {
		*yposd += fr->lines[i].height;
	}

	*line_height = pango_units_to_double(fr->lines[line].height);
	*yposd /= PANGO_SCALE;
	*yposd += fr->lop.pad_t;

	l = &fr->lines[line];
	box = 0;
	for ( i=0; i<l->n_boxes-1; i++ ) {
		box = i;
		if ( l->boxes[i+1].type == WRAP_BOX_SENTINEL ) break;
		if ( l->boxes[i+1].sc_offset > pos ) break;
		*xposd += l->boxes[i].width;
		if ( i < l->n_boxes-2 ) {
			*xposd += l->boxes[i].sp;
		}
	}

	b = &l->boxes[box];
	if ( b->type == WRAP_BOX_PANGO ) {
		pango_glyph_string_index_to_x(b->glyphs, b->text,
		                              strlen(b->text),
			                      &b->item->analysis,
			                      pos - b->sc_offset,
			                      FALSE, &p);
		//printf("offset %i in '%s' -> %i\n",
		//       (int)pos-(int)b->sc_offset, b->text, p);

		*xposd += p;
		*xposd /= PANGO_SCALE;
		*xposd += fr->lop.pad_l;
		//printf("%i  ->  line %i, box %i  -> %f, %f\n",
		//       (int)pos, line, box, *xposd, *yposd);

	} else {
	}
}
#endif


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

	for ( i=0; i<line->n_boxes; i++ ) {
		struct wrap_box *box = &line->boxes[i];
		line->width += box->width;
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
	box->ascent = 0;
	box->height = 0;
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

			if ( add_wrap_box(boxes, word, start, type, pc,
			                  font) ) {
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
			add_wrap_box(boxes, word2, start, WRAP_SPACE_EOP, pc,
		                     font);
			add_wrap_box(boxes, strdup(""), i, WRAP_SPACE_NONE, pc,
		                     font);

		} else {

			add_wrap_box(boxes, word, start, WRAP_SPACE_NONE, pc,
				     font);

		}

	}

	free(log_attrs);
	return 0;
}


static void set_font(struct sc_font *scf, const char *font_name,
                     PangoContext *pc)
{
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
	scf->col[0] = 0.0;  scf->col[1] = 0.0;  scf->col[2] = 0.0;
	scf->col[3] = 1.0;
}



static struct sc_font *push_font(struct sc_font *stack, const char *font_name,
                                 int *n_fonts, int *max_fonts,
                                 PangoContext *pc)
{
	if ( *n_fonts == *max_fonts ) {

		struct sc_font *stack_new;

		stack_new = realloc(stack,
		                    sizeof(struct sc_font)*((*max_fonts)+8));
		if ( stack_new == NULL ) {
			fprintf(stderr, "Failed to push font.\n");
			return stack;
		}

		stack = stack_new;
		*max_fonts += 8;

	}

	set_font(&stack[*n_fonts], font_name, pc);
	(*n_fonts)++;

	return stack;
}


static void pop_font(struct sc_font *stack, int *n_fonts, int *max_fonts)
{
	pango_font_description_free(stack[(*n_fonts)-1].fontdesc);
	(*n_fonts)--;
}


static void run_sc(const char *sc, struct sc_font *fonts, int *n_fonts,
                   int *max_fonts, PangoContext *pc, struct wrap_line *boxes,
                   PangoLanguage *lang)
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

		if ( b->name == NULL ) {
			if ( split_words(boxes, pc, b->contents, b->offset,
			                 lang, &fonts[(*n_fonts)-1]) ) {
				fprintf(stderr, "Splitting failed.\n");
			}

		} else if ( (strcmp(b->name, "font")==0)
		         && (b->contents == NULL) ) {
			set_font(&fonts[(*n_fonts)-1], b->options, pc);

		} else if ( (strcmp(b->name, "font")==0)
		         && (b->contents != NULL) ) {
			push_font(fonts, b->options, n_fonts, max_fonts, pc);
			run_sc(b->contents, fonts, n_fonts, max_fonts, pc,
			       boxes, lang);
			pop_font(fonts, n_fonts, max_fonts);
		}

		/* FIXME: Handle images */

	}
	sc_block_list_free(bl);
}


static struct wrap_line *sc_to_wrap_boxes(const char *sc, const char *prefix,
                                          PangoContext *pc)
{
	struct wrap_line *boxes;
	PangoLanguage *lang;
	struct sc_font *fonts;
	int n_fonts, max_fonts;
	int i;

	boxes = malloc(sizeof(struct wrap_line));
	if ( boxes == NULL ) {
		fprintf(stderr, "Failed to allocate boxes.\n");
		return NULL;
	}
	initialise_line(boxes);

	fonts = NULL;
	n_fonts = 0;
	max_fonts = 0;

	/* FIXME: Determine proper language (somehow...) */
	lang = pango_language_from_string("en_GB");

	/* The "ultimate" default font */
	fonts = push_font(fonts, "Sans 12", &n_fonts, &max_fonts, pc);

	if ( prefix != NULL ) {
		run_sc(prefix, fonts, &n_fonts, &max_fonts, pc, boxes, lang);
	}
	run_sc(sc, fonts, &n_fonts, &max_fonts, pc, boxes, lang);

	for ( i=0; i<n_fonts; i++ ) {
		pango_font_description_free(fonts[i].fontdesc);
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
