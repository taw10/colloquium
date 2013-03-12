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
	signed int line;
	int i;

	*xposd = 0;
	*yposd = 0;

	line = 0;
	for ( i=0; i<fr->n_lines; i++ ) {
		line = i;
		if ( fr->lines[i].sc_offset > pos ) {
			line = i-1;
			break;
		}
		*yposd += fr->lines[i].height;
		*line_height = pango_units_to_double(fr->lines[i].height);
	}
	assert(line >= 0);

	*xposd += fr->lop.pad_l;

	*yposd /= PANGO_SCALE;
	*yposd += fr->lop.pad_t;
}


static void shape_and_measure(gpointer data, gpointer user_data)
{
	struct wrap_box *box = user_data;
	PangoItem *item = data;
	PangoRectangle rect;

	/* FIXME: Don't assume only one run per wrap box */
	box->glyphs = pango_glyph_string_new();

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

	pango_item_free(item);
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


/* Add "text", followed by a space of type "space", to "line" */
static int add_wrap_box(struct wrap_line *line, char *text,
                        enum wrap_box_space space, PangoContext *pc,
                        PangoFont *font, PangoFontDescription *fontdesc,
                        double col[4])
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
	box->type = WRAP_BOX_PANGO;
	box->text = text;
	box->space = space;
	box->font = font;
	box->width = 0;
	box->ascent = 0;
	box->height = 0;
	box->col[0] = col[0];  /* Red */
	box->col[1] = col[1];  /* Green */
	box->col[2] = col[2];  /* Blue */
	box->col[3] = col[3];  /* Alpha */
	line->n_boxes++;

	attrs = pango_attr_list_new();
	attr = pango_attr_font_desc_new(fontdesc);
	pango_attr_list_insert_before(attrs, attr);
	pango_items = pango_itemize(pc, text, 0, strlen(text), attrs, NULL);
	g_list_foreach(pango_items, shape_and_measure, box);
	g_list_free(pango_items);
	pango_attr_list_unref(attrs);

	return 0;
}


static int split_words(struct wrap_line *boxes, PangoContext *pc, char *sc,
                       PangoLanguage *lang,
                       PangoFont *font, PangoFontDescription *fontdesc,
                       double col[4])
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

			if ( add_wrap_box(boxes, word, type, pc, font, fontdesc,
			                  col) ) {
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

		add_wrap_box(boxes, word, WRAP_SPACE_NONE, pc, font, fontdesc,
		             col);

	}

	free(log_attrs);
	return 0;
}


static struct wrap_line *sc_to_wrap_boxes(const char *sc, PangoContext *pc)
{
	struct wrap_line *boxes;
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;
	PangoFontDescription *fontdesc;
	PangoFont *font;
	double col[4];
	PangoLanguage *lang;

	boxes = malloc(sizeof(struct wrap_line));
	if ( boxes == NULL ) {
		fprintf(stderr, "Failed to allocate boxes.\n");
		return NULL;
	}
	initialise_line(boxes);

	bl = sc_find_blocks(sc, NULL);

	if ( bl == NULL ) {
		printf("Failed to find blocks.\n");
		return NULL;
	}

	/* FIXME: Determine proper language (somehow...) */
	lang = pango_language_from_string("en_GB");

	/* FIXME: Determine the proper font to use */
	fontdesc = pango_font_description_from_string("Sorts Mill Goudy 16");
	if ( fontdesc == NULL ) {
		fprintf(stderr, "Couldn't describe font.\n");
		return NULL;
	}
	font = pango_font_map_load_font(pango_context_get_font_map(pc),
	                                pc, fontdesc);
	if ( font == NULL ) {
		fprintf(stderr, "Couldn't load font.\n");
		return NULL;
	}
	col[0] = 0.0;  col[1] = 0.0;  col[2] = 0.0;  col[3] = 1.0;

	/* Iterate through SC blocks and send each one in turn for processing */
	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		if ( b->name == NULL ) {
			if ( split_words(boxes, pc, b->contents, lang,
			                 font, fontdesc, col) ) {
				fprintf(stderr, "Splitting failed.\n");
			}
		}

		/* FIXME: Handle images */
	}
	sc_block_list_free(bl);

	pango_font_description_free(fontdesc);

	return boxes;
}


/* Normal width of space */
static double sp_x(enum wrap_box_space s)
{
	switch ( s ) {

		case WRAP_SPACE_INTERWORD :
		return 20.0*PANGO_SCALE;

		case WRAP_SPACE_EOP :
		default:
		return 0.0;

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
		default:
		return 0.0;

		case WRAP_SPACE_NONE :
		return INFINITY;

	}
}


/* Shrinkability of space */
static double sp_z(enum wrap_box_space s)
{
	switch ( s ) {

		case WRAP_SPACE_INTERWORD :
		return 7.0*PANGO_SCALE;

		case WRAP_SPACE_EOP :
		default:
		return 0.0;

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


static void distribute_spaces(struct wrap_line *l, double w)
{
	int i;
	double total;
	double sp;
	int overfull = 0;

	total = 0.0;
	for ( i=0; i<l->n_boxes; i++ ) {
		total += l->boxes[i].width;
	}

	sp = (pango_units_from_double(w)-total) / (l->n_boxes-1);

	for ( i=0; i<l->n_boxes-1; i++ ) {
		if ( sp < sp_zp(l->boxes[i].space) ) {
			l->boxes[i].sp = sp_zp(l->boxes[i].space);
			overfull = 1;
		} else if ( l->last_line ) {
			l->boxes[i].sp = sp_x(l->boxes[i].space);
		} else {
			l->boxes[i].sp = sp;
		}
	}
	l->boxes[l->n_boxes-1].sp = 0.0;
	l->overfull = overfull;
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
                                 struct frame *fr)
{
	int a = 0;
	int *p;
	double *s;
	const double rho = 2.0;
	struct wrap_box *box;
	int j;
	double sigma_prime, sigma_max_prime, sigma_min_prime;
	double dprime;
	int n;
	int reject;

	fr->n_lines = 0;
	fr->max_lines = 32;
	fr->lines = NULL;
	alloc_lines(fr);

	/* Add empty zero-width box at end */
	if ( boxes->n_boxes == boxes->max_boxes ) {
		boxes->max_boxes += 32;
		alloc_boxes(boxes);
		if ( boxes->n_boxes == boxes->max_boxes ) return;
	}
	box = &boxes->boxes[boxes->n_boxes];
	box->type = WRAP_BOX_NOTHING;
	box->text = NULL;
	box->space = WRAP_SPACE_NONE;
	box->font = NULL;
	box->width = 0;
	box->ascent = 0;
	box->height = 0;
	n = boxes->n_boxes-1;
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

	p = malloc(boxes->n_boxes * sizeof(int));
	if ( p == NULL ) {
		fprintf(stderr, "Failed to allocate p_k\n");
		return;
	}

	s = malloc(boxes->n_boxes * sizeof(double));
	if ( s == NULL ) {
		fprintf(stderr, "Failed to allocate s_k\n");
		return;
	}

	do {

		int i = a;
		int k = i+1;
		double sigma = boxes->boxes[k].width;
		double sigma_min = sigma;
		double sigma_max = sigma;
		int m = 1;
		int jprime;

		s[i] = 0;

		do {

			while ( sigma_min > line_length ) {

				/* Begin <advance i by 1> */
				if ( s[i] < INFINITY ) m--;
				i++;
				sigma -= boxes->boxes[i].width;
				sigma -= sp_z(boxes->boxes[i].space);

				sigma_max -= boxes->boxes[i].width;
				sigma_max -= sp_yp(boxes->boxes[i].space, rho);

				sigma_min -= boxes->boxes[i].width;
				sigma_min -= sp_zp(boxes->boxes[i].space);
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
				sigma_prime -= boxes->boxes[j].width;
				sigma_prime -= sp_x(boxes->boxes[j].space);

				sigma_max_prime -= boxes->boxes[j].width;
				sigma_max_prime -= sp_yp(boxes->boxes[j].space,
					                 rho);

				sigma_min_prime -= boxes->boxes[j].width;
				sigma_min_prime -= sp_zp(boxes->boxes[j].space);
			}
			/* End */

			s[k] = dprime;
			if ( dprime < INFINITY ) {
				m++;
				p[k] = jprime;
			}
			if ( (m == 0) || (k > n) ) break;
			sigma += boxes->boxes[k+1].width;
			sigma += sp_x(boxes->boxes[k].space);

			sigma_max += boxes->boxes[k+1].width;
			sigma_max += sp_yp(boxes->boxes[k].space, rho);

			sigma_min += boxes->boxes[k+1].width;
			sigma_min += sp_zp(boxes->boxes[k].space);
			k++;

		} while ( 1 );

		if ( k > n ) {
			output(a, n+1, p, fr, boxes);
			break;
		} else {

			/* Begin <try hyphenation> */
			do {

				sigma += boxes->boxes[i].width;
				sigma += sp_x(boxes->boxes[i].space);

				sigma_max += boxes->boxes[i].width;
				sigma_max += sp_yp(boxes->boxes[i].space, rho);

				sigma_min += boxes->boxes[i].width;
				sigma_min += sp_zp(boxes->boxes[i].space);
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


/* Wrap the StoryCode inside "fr->sc" so that it fits within width "fr->w",
 * and generate fr->lines */
int wrap_contents(struct frame *fr, PangoContext *pc)
{
	struct wrap_line *boxes;
	int i;

	/* Turn the StoryCode into wrap boxes, all on one line */
	boxes = sc_to_wrap_boxes(fr->sc, pc);
	if ( boxes == NULL ) {
		fprintf(stderr, "Failed to create wrap boxes.\n");
		return 1;
	}

	knuth_suboptimal_fit(boxes, fr->w - fr->lop.pad_l - fr->lop.pad_r, fr);

	for ( i=0; i<fr->n_lines; i++ ) {
		distribute_spaces(&fr->lines[i], fr->w);
		calc_line_geometry(&fr->lines[i]);
	}

	return 0;
}

