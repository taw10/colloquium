/*
 * wrap.c
 *
 * Text wrapping, hyphenation, justification etc
 *
 * Copyright © 2013-2015 Thomas White <taw@bitwiz.org.uk>
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

#include "sc_parse.h"
#include "sc_interp.h"
#include "wrap.h"
#include "frame.h"
#include "presentation.h"


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


void alloc_boxes(struct wrap_line *l)
{
	struct wrap_box *boxes_new;

	boxes_new = realloc(l->boxes, l->max_boxes * sizeof(struct wrap_box));
	if ( boxes_new == NULL ) {
		fprintf(stderr, "Couldn't allocate memory for boxes!\n");
		return;
	}

	l->boxes = boxes_new;
}


void initialise_line(struct wrap_line *l)
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


#if 0
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
	*yposd += fr->pad_t;

	return &fr->lines[line];
}
#endif

void get_cursor_pos(struct wrap_box *box, int pos,
                    double *xposd, double *yposd, double *line_height)
{
	int p;
	const char *block_text;
	const char *box_text;
	const char *ep;

	*xposd = 0.0;
	*yposd = 0.0;
	*line_height = 20.0;

	if ( box == NULL ) return;

	*line_height = pango_units_to_double(box->height);

	if ( !box->editable ) {
		*xposd += pango_units_to_double(box->width);
		return;
	}

	switch ( box->type ) {

		case WRAP_BOX_PANGO :
		block_text = sc_block_contents(box->scblock);
		box_text = g_utf8_offset_to_pointer(block_text, box->offs_char);
		ep = g_utf8_offset_to_pointer(box_text, box->len_chars);
		pango_glyph_string_index_to_x(box->glyphs, (char *)box_text,
		                              ep - box_text, &box->analysis,
			                      pos, FALSE, &p);
		*xposd += pango_units_to_double(p);
		break;

		case WRAP_BOX_IMAGE :
		if ( pos > 0 ) {
			*xposd += pango_units_to_double(box->width);
		} /* else zero */
		break;

		case WRAP_BOX_CALLBACK :
		if ( pos > 0 ) {
			*xposd += pango_units_to_double(box->width);
		}
		break;

		case WRAP_BOX_NOTHING :
		case WRAP_BOX_SENTINEL :
		*xposd = 0.0;
		break;

	}
}


static int find_cursor_line(struct frame *fr, double yposd, int *end)
{
	int i;
	double y = fr->pad_t;

	*end = 0;

	for ( i=0; i<fr->n_lines; i++ ) {
		double height = pango_units_to_double(fr->lines[i].height);
		if ( yposd < y + height ) {
			return i;
		}
		y += height;
	}

	*end = 1;
	return fr->n_lines-1;
}


static int find_cursor_box(struct frame *fr, struct wrap_line *l,
			   double xposd, double *x_pos, int *end)
{
	int i;
	double x = fr->pad_l;

	*end = 0;

	for ( i=0; i<l->n_boxes; i++ ) {
		double width = pango_units_to_double(l->boxes[i].width);
		width += pango_units_to_double(l->boxes[i].sp);
		if ( xposd < x + width ) {
			*x_pos = xposd - x;
			return i;
		}
		x += width;
	}

	*end = 1;
	*x_pos = x;
	return l->n_boxes-1;
}


void find_cursor(struct frame *fr, double xposd, double yposd,
                 int *line, int *box, int *pos)
{
	struct wrap_line *l;
	struct wrap_box *b;
	int end;
	double x_pos = 0.0;
	int idx, trail;
	int x_pos_i;
	size_t offs;
	int ln, bn;
	const char *block_text;
	const char *box_text;

	if ( fr->n_lines == 0 ) {
		*line = 0;
		*box = 0;
		*pos = 0;
		return;
	}

	ln = find_cursor_line(fr, yposd, &end);
	l = &fr->lines[ln];
	*line = ln;

	if ( end ) {
		bn = l->n_boxes-1;
	} else {
		bn = find_cursor_box(fr, l, xposd, &x_pos, &end);
	}
	b = &l->boxes[bn];
	*box = bn;
	if ( end ) {
		*pos = b->len_chars;
		return;
	}

	if ( !b->editable ) {
		*pos = 0;
		return;
	}

	switch ( b->type ) {

		case WRAP_BOX_NOTHING:
		case WRAP_BOX_SENTINEL:
		case WRAP_BOX_IMAGE:
		case WRAP_BOX_CALLBACK:
		offs = 0;
		break;

		case WRAP_BOX_PANGO:
		x_pos_i = pango_units_from_double(x_pos);
		if ( x_pos_i < b->width ) {
			block_text = sc_block_contents(b->scblock);
			box_text = g_utf8_offset_to_pointer(block_text,
			                                    b->offs_char);
			printf("box text '%s'\n", box_text);
			/* cast because this function is not const-clean */
			pango_glyph_string_x_to_index(b->glyphs,
			                              (char *)box_text,
			                              strlen(box_text),
			                              &b->analysis,
			                              x_pos_i, &idx, &trail);
			offs = idx + trail;
			/* FIXME: Bug in Pango? */
			if ( offs > b->len_chars ) offs = b->len_chars;
		} else {
			offs = 0;
		};
		break;

		default:
		fprintf(stderr, "find_cursor(): box %i\n", b->type);
		offs = 0;
		break;

	}

	*pos = offs;
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

	line->height *= 1.07;
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


static void UNUSED distribute_spaces(struct wrap_line *line, double l,
                                     double rho)
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
static void UNUSED knuth_suboptimal_fit(struct wrap_line *boxes,
                                        double line_length, struct frame *fr,
                                        double rho)
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
	box->space = WRAP_SPACE_NONE;
	box->font = NULL;
	box->width = 0;
	box->ascent = 0;
	box->height = 0;
	box->editable = 1;
	box->scblock = NULL;
	box->offs_char = 0;
	boxes->n_boxes++;

	line_length *= PANGO_SCALE;

	reject = 0;
	for ( j=0; j<boxes->n_boxes; j++ ) {
		if ( boxes->boxes[j].width > line_length ) {
			fprintf(stderr, "ERROR: Box %i too long (%i %f)\n", j,
			                boxes->boxes[j].width, line_length);
			fr->trouble = 1;
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


static struct wrap_line *new_line(struct frame *fr)
{
	struct wrap_line *l;

	if ( fr->n_lines + 1 > fr->max_lines ) {
		fr->max_lines += 32;
		alloc_lines(fr);
		if ( fr->n_lines == fr->max_lines ) return NULL;
	}

	l = &fr->lines[fr->n_lines];
	fr->n_lines++;
	initialise_line(l);
	return l;
}


static void first_fit(struct wrap_line *boxes, double line_length,
                      struct frame *fr)
{
	struct wrap_line *line;
	double len;
	int j = 0;

	line_length *= PANGO_SCALE;

	line = new_line(fr);
	len = 0.0;

	do {

		boxes->boxes[j].sp = sp_x(boxes->boxes[j].space);

		len += boxes->boxes[j].width;

		if ( len > line_length ) {
			line = new_line(fr);
			len = boxes->boxes[j].width;
		}
		line->boxes[line->n_boxes] = boxes->boxes[j];
		line->boxes[line->n_boxes].cf = &boxes->boxes[j];
		line->n_boxes++;
		j++;

		if ( (j > 0) && (boxes->boxes[j-1].type != WRAP_BOX_SENTINEL) )
		{
			len += sp_x(boxes->boxes[j-1].space);
		}

	} while ( j < boxes->n_boxes );

	fr->lines[fr->n_lines-1].last_line = 1;
}


void wrap_line_free(struct wrap_line *l)
{
	int i;
	for ( i=0; i<l->n_boxes; i++ ) {

		switch ( l->boxes[i].type ) {

			case WRAP_BOX_PANGO :
			pango_glyph_string_free(l->boxes[i].glyphs);
			break;

			case WRAP_BOX_IMAGE :
			break;

			case WRAP_BOX_CALLBACK :
			break;

			case WRAP_BOX_NOTHING :
			case WRAP_BOX_SENTINEL :
			break;

		}

	}

	free(l->boxes);
}


static struct wrap_line *split_paragraph(struct wrap_line *boxes, int *n,
                                         int *eop)
{
	int i;
	int start = *n;
	int end;

	if ( start >= boxes->n_boxes ) return NULL;

	*eop = 0;
	for ( i=start; i<boxes->n_boxes; i++ ) {
		if ( boxes->boxes[i].space == WRAP_SPACE_EOP ) {
			*eop = 1;
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
			para->boxes[i-start].cf = &boxes->boxes[i];
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
		struct wrap_box *box = &boxes->boxes[i];
		char *box_text;
		printf("%3i", i);
		if ( box->scblock != NULL ) {
			const char *block_text = sc_block_contents(box->scblock);
			box_text = g_utf8_offset_to_pointer(block_text,
			                                    box->offs_char);
		} else if ( box->type == WRAP_BOX_IMAGE ) {
			box_text = box->filename;
		} else {
			box_text = NULL;
		}
		printf(" t=%i s=%i %10i %5i %8.2f '%s'\n",
		       box->type, box->space, box->width, box->offs_char,
		       box->sp, box_text);
	}
}


/* Wrap the StoryCode inside "fr->sc" so that it fits within width "fr->w",
 * and generate fr->lines */
int wrap_contents(struct frame *fr)
{
	struct wrap_line *para;
	int i, eop = 0;
	//const double rho = 2.0;
	const double wrap_w = fr->w - fr->pad_l - fr->pad_r;
	int max_para = 64;

	/* Clear lines */
	fr->n_lines = 0;
	fr->max_lines = 32;
	fr->lines = NULL;
	fr->trouble = 0;
	alloc_lines(fr);

	/* Split text into paragraphs */
	i = 0;
	fr->n_paragraphs = 0;
	fr->paragraphs = malloc(max_para * sizeof(struct wrap_line *));
	if ( fr->paragraphs == NULL ) {
		fprintf(stderr, "Failed to allocate paragraphs\n");
		return 1;
	}
	do {
		para = split_paragraph(fr->boxes, &i, &eop);
		if ( para != NULL ) {
			fr->paragraphs[fr->n_paragraphs++] = para;
		}
		if ( fr->n_paragraphs == max_para ) {
			max_para += 64;
			fr->paragraphs = realloc(fr->paragraphs,
			                   max_para*sizeof(struct wrap_line *));
			if ( fr->paragraphs == NULL ) {
				fprintf(stderr, "Failed to allocate space"
				                " for paragraphs\n");
				return 1;
			}
		}
	} while ( para != NULL );

	/* Split paragraphs into lines */
	for ( i=0; i<fr->n_paragraphs; i++ ) {
		/* Choose wrapping algorithm here */
		//knuth_suboptimal_fit(para, wrap_w, fr, rho);
		first_fit(fr->paragraphs[i], wrap_w, fr);
	}

	/* If the last paragraph ended with an EOP, add an extra line */
	if ( eop || (fr->n_lines == 0) ) {

		struct wrap_line *l;
		struct wrap_box *last_box;

		if ( fr->n_lines > 0 ) {
			l = &fr->lines[fr->n_lines-1];
			last_box = &l->boxes[l->n_boxes-1];
		} else {
			last_box = NULL;
		}

		if ( fr->n_lines + 1 > fr->max_lines ) {
			fr->max_lines += 32;
			alloc_lines(fr);
			if ( fr->n_lines == fr->max_lines ) return 1;
		}

		l = &fr->lines[fr->n_lines];
		fr->n_lines++;
		initialise_line(l);

		l->max_boxes = 1;
		alloc_boxes(l);
		l->n_boxes = 1;
		l->boxes[0].type = WRAP_BOX_NOTHING;
		l->boxes[0].editable = 1;
		l->boxes[0].space = WRAP_SPACE_NONE;

		if ( last_box != NULL ) {
			l->boxes[0].scblock = last_box->scblock;
			l->boxes[0].offs_char = last_box->len_chars
			                        + last_box->offs_char;
			l->boxes[0].ascent = last_box->ascent;
			l->boxes[0].height = last_box->height;
			l->boxes[0].width = 0;
			/* FIXME: Get ascent and descent from font metrics for
			 * whichever font will be used in this box */
		} else {
			l->boxes[0].scblock = find_last_child(fr->scblocks);
			l->boxes[0].offs_char = 0;
			l->boxes[0].ascent = 10000;
			l->boxes[0].height = 10000;
			l->boxes[0].width = 0;
		}
	}

	for ( i=0; i<fr->n_lines; i++ ) {

		struct wrap_line *line = &fr->lines[i];

		//distribute_spaces(line, wrap_w, rho);

		/* Strip any sentinel boxes added by the wrapping algorithm */
		if ( line->boxes[line->n_boxes-1].type == WRAP_BOX_SENTINEL ) {
			line->n_boxes--;
		}

	}

	for ( i=0; i<fr->n_lines; i++ ) {
		calc_line_geometry(&fr->lines[i]);
	}

	return 0;
}
