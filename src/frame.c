/*
 * frame.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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
#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "sc_parse.h"
#include "frame.h"
#include "imagestore.h"

struct text_run
{
	SCBlock              *scblock;   /* If macro, this is \macro */
	SCBlock              *rscblock;  /* The block with the actual text */
	size_t                para_offs_bytes;  /* Offset from start of paragraph */
	size_t                len_bytes;
	PangoFontDescription *fontdesc;
	double                col[4];
};

struct _paragraph
{
	enum para_type   type;
	double           height;
	float            space[4];
	SCBlock         *newline_at_end;

	/* For PARA_TYPE_TEXT */
	int              n_runs;
	struct text_run *runs;
	int              open;
	PangoLayout     *layout;
	size_t           offset_last;

	/* For anything other than PARA_TYPE_TEXT
	 * (for text paragraphs, these things are in the runs) */
	SCBlock         *scblock;

	/* For PARA_TYPE_IMAGE */
	char            *filename;
	double           image_w;
	double           image_h;
	int              image_real_w;
	int              image_real_h;

	/* For PARA_TYPE_CALLBACK */
	double                cb_w;
	double                cb_h;
	SCCallbackDrawFunc    draw_func;
	SCCallbackClickFunc   click_func;
	void                 *bvp;
	void                 *vp;
};


PangoLayout *paragraph_layout(Paragraph *para)
{
	return para->layout;
}


double paragraph_height(Paragraph *para)
{
	return para->height;
}


static int alloc_ro(struct frame *fr)
{
	struct frame **new_ro;

	new_ro = realloc(fr->children,
	                 fr->max_children*sizeof(struct frame *));
	if ( new_ro == NULL ) return 1;

	fr->children = new_ro;

	return 0;
}


struct frame *frame_new()
{
	struct frame *n;

	n = calloc(1, sizeof(struct frame));
	if ( n == NULL ) return NULL;

	n->children = NULL;
	n->max_children = 32;
	if ( alloc_ro(n) ) {
		fprintf(stderr, "Couldn't allocate children\n");
		free(n);
		return NULL;
	}
	n->num_children = 0;

	n->scblocks = NULL;
	n->n_paras = 0;
	n->paras = NULL;

	return n;
}


static void free_paragraph(Paragraph *para)
{
	int i;

	for ( i=0; i<para->n_runs; i++ ) {
		pango_font_description_free(para->runs[i].fontdesc);
	}
	free(para->runs);
	if ( para->layout != NULL ) g_object_unref(para->layout);
	free(para);
}


void frame_free(struct frame *fr)
{
	int i;

	if ( fr == NULL ) return;

	/* Free paragraphs */
	if ( fr->paras != NULL ) {
		for ( i=0; i<fr->n_paras; i++ ) {
			free_paragraph(fr->paras[i]);
		}
		free(fr->paras);
	}

	/* Free all children */
	for ( i=0; i<fr->num_children; i++ ) {
		frame_free(fr->children[i]);
	}
	free(fr->children);

	free(fr);
}


struct frame *add_subframe(struct frame *fr)
{
	struct frame *n;

	n = frame_new();
	if ( n == NULL ) return NULL;

	if ( fr->num_children == fr->max_children ) {
		fr->max_children += 32;
		if ( alloc_ro(fr) ) return NULL;
	}

	fr->children[fr->num_children++] = n;

	return n;
}


void show_hierarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];

	strcpy(tn, t);
	strcat(tn, "      ");

	printf("%s%p (%.2f x %.2f)\n", t, fr, fr->w, fr->h);

	for ( i=0; i<fr->num_children; i++ ) {
		show_hierarchy(fr->children[i], tn);
	}

}


static struct frame *find_parent(struct frame *fr, struct frame *search)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {
		if ( fr->children[i] == search ) {
			return fr;
		}
	}

	for ( i=0; i<fr->num_children; i++ ) {
		struct frame *tt;
		tt = find_parent(fr->children[i], search);
		if ( tt != NULL ) return tt;
	}

	return NULL;
}


void delete_subframe(struct frame *top, struct frame *fr)
{
	struct frame *parent;
	int i, idx, found;

	parent = find_parent(top, fr);
	if ( parent == NULL ) {
		fprintf(stderr, "Couldn't find parent when deleting frame.\n");
		return;
	}

	found = 0;
	for ( i=0; i<parent->num_children; i++ ) {
		if ( parent->children[i] == fr ) {
			idx = i;
			found = 1;
			break;
		}
	}

	if ( !found ) {
		fprintf(stderr, "Couldn't find child when deleting frame.\n");
		return;
	}

	for ( i=idx; i<parent->num_children-1; i++ ) {
		parent->children[i] = parent->children[i+1];
	}

	parent->num_children--;
}


struct frame *find_frame_with_scblocks(struct frame *fr, SCBlock *scblocks)
{
	int i;

	if ( fr->scblocks == scblocks ) return fr;

	for ( i=0; i<fr->num_children; i++ ) {
		struct frame *tt;
		tt = find_frame_with_scblocks(fr->children[i], scblocks);
		if ( tt != NULL ) return tt;
	}

	return NULL;
}


void wrap_paragraph(Paragraph *para, PangoContext *pc, double w,
                    size_t sel_start, size_t sel_end)
{
	size_t total_len = 0;
	int i;
	char *text;
	PangoAttrList *attrs;
	PangoRectangle rect;
	size_t pos = 0;

	w -= para->space[0] + para->space[1];

	if ( para->type == PARA_TYPE_IMAGE ) {
		if ( para->image_w < 0.0 ) {
			para->image_w = w;
			para->image_h = w*((float)para->image_real_h/para->image_real_w);
		}
		para->height = para->image_h;
		return;
	}

	if ( para->type != PARA_TYPE_TEXT ) return;

	for ( i=0; i<para->n_runs; i++ ) {
		total_len += para->runs[i].len_bytes;
	}

	/* Allocate the complete text */
	text = malloc(total_len+1);
	if ( text == NULL ) {
		fprintf(stderr, "Couldn't allocate combined text (%lli)\n",
		       (long long int)total_len);
		return;
	}

	/* Allocate the attributes */
	attrs = pango_attr_list_new();

	/* Put all of the text together */
	text[0] = '\0';
	for ( i=0; i<para->n_runs; i++ ) {

		PangoAttribute *attr;
		const char *run_text;
		guint16 r, g, b;

		run_text = sc_block_contents(para->runs[i].rscblock);

		attr = pango_attr_font_desc_new(para->runs[i].fontdesc);
		attr->start_index = pos;
		attr->end_index = pos + para->runs[i].len_bytes;
		pango_attr_list_insert(attrs, attr);

		r = para->runs[i].col[0] * 65535;
		g = para->runs[i].col[1] * 65535;
		b = para->runs[i].col[2] * 65535;
		attr = pango_attr_foreground_new(r, g, b);
		attr->start_index = pos;
		attr->end_index = pos + para->runs[i].len_bytes;
		pango_attr_list_insert(attrs, attr);

		pos += para->runs[i].len_bytes;
		strncat(text, run_text, para->runs[i].len_bytes);

	}

	/* Add attributes for selected text */
	if ( sel_start > 0 || sel_end > 0 ) {
		PangoAttribute *attr;
		attr = pango_attr_background_new(42919, 58853, 65535);
		attr->start_index = sel_start;
		attr->end_index = sel_end;
		pango_attr_list_insert(attrs, attr);
	}

	if ( para->layout == NULL ) {
		para->layout = pango_layout_new(pc);
		pango_layout_set_spacing(para->layout, 5000);
	}
	pango_layout_set_width(para->layout, pango_units_from_double(w));
	pango_layout_set_text(para->layout, text, total_len);
	pango_layout_set_attributes(para->layout, attrs);
	free(text);
	pango_attr_list_unref(attrs);

	pango_layout_get_extents(para->layout, NULL, &rect);
	para->height = pango_units_to_double(rect.height);
	para->height += para->space[2] + para->space[3];
}

SCBlock *get_newline_at_end(Paragraph *para)
{
	return para->newline_at_end;
}


void set_newline_at_end(Paragraph *para, SCBlock *bl)
{
	para->newline_at_end = bl;
}


void add_run(Paragraph *para, SCBlock *scblock, SCBlock *rscblock,
             size_t len_bytes, PangoFontDescription *fdesc,
             double col[4])
{
	struct text_run *runs_new;

	if ( !para->open ) {
		fprintf(stderr, "Adding a run to a closed paragraph!\n");
		return;
	}

	runs_new = realloc(para->runs,
	                   (para->n_runs+1)*sizeof(struct text_run));
	if ( runs_new == NULL ) {
		fprintf(stderr, "Failed to add run.\n");
		return;
	}

	para->runs = runs_new;
	para->runs[para->n_runs].scblock = scblock;
	para->runs[para->n_runs].rscblock = rscblock;
	para->runs[para->n_runs].para_offs_bytes = para->offset_last;
	para->offset_last += len_bytes;
	para->runs[para->n_runs].len_bytes = len_bytes;
	para->runs[para->n_runs].fontdesc = pango_font_description_copy(fdesc);
	para->runs[para->n_runs].col[0] = col[0];
	para->runs[para->n_runs].col[1] = col[1];
	para->runs[para->n_runs].col[2] = col[2];
	para->runs[para->n_runs].col[3] = col[3];
	para->n_runs++;
}


static Paragraph *create_paragraph(struct frame *fr)
{
	Paragraph **paras_new;
	Paragraph *pnew;

	paras_new = realloc(fr->paras, (fr->n_paras+1)*sizeof(Paragraph *));
	if ( paras_new == NULL ) return NULL;

	pnew = calloc(1, sizeof(struct _paragraph));
	if ( pnew == NULL ) return NULL;

	fr->paras = paras_new;
	fr->paras[fr->n_paras++] = pnew;

	return pnew;
}


/* Create a new paragraph in 'fr' just after paragraph 'pos' */
Paragraph *insert_paragraph(struct frame *fr, int pos)
{
	Paragraph **paras_new;
	Paragraph *pnew;
	int i;

	if ( pos >= fr->n_paras ) {
		fprintf(stderr, "insert_paragraph(): pos too high!\n");
		return NULL;
	}

	paras_new = realloc(fr->paras, (fr->n_paras+1)*sizeof(Paragraph *));
	if ( paras_new == NULL ) return NULL;

	pnew = calloc(1, sizeof(struct _paragraph));
	if ( pnew == NULL ) return NULL;

	pnew->open = 1;

	fr->paras = paras_new;
	fr->n_paras++;

	for ( i=fr->n_paras-1; i>pos; i-- ) {
		fr->paras[i] = fr->paras[i-1];
	}
	fr->paras[pos+1] = pnew;

	return pnew;
}


void add_callback_para(struct frame *fr, SCBlock *bl,
                       double w, double h,
                       SCCallbackDrawFunc draw_func,
                       SCCallbackClickFunc click_func, void *bvp,
                       void *vp)
{
	Paragraph *pnew;

	pnew = create_paragraph(fr);
	if ( pnew == NULL ) {
		fprintf(stderr, "Failed to add callback paragraph\n");
		return;
	}

	pnew->type = PARA_TYPE_CALLBACK;
	pnew->scblock = bl;
	pnew->cb_w = w;
	pnew->cb_h = h;
	pnew->draw_func = draw_func;
	pnew->click_func = click_func;
	pnew->bvp = bvp;
	pnew->vp = vp;
	pnew->height = h;
	pnew->open = 0;
}


void add_image_para(struct frame *fr, SCBlock *scblock, const char *filename,
                    ImageStore *is, double w, double h, int editable)
{
	Paragraph *pnew;
	int wi, hi;

	pnew = create_paragraph(fr);
	if ( pnew == NULL ) {
		fprintf(stderr, "Failed to add image paragraph\n");
		return;
	}

	if ( imagestore_get_size(is, filename, &wi, &hi) ) {
		fprintf(stderr, "Couldn't get size for %s\n", filename);
		wi = 100;
		hi = 100;
	}

	pnew->type = PARA_TYPE_IMAGE;
	pnew->scblock = scblock;
	pnew->filename = strdup(filename);
	pnew->image_w = w;
	pnew->image_h = h;
	pnew->image_real_w = wi;
	pnew->image_real_h = hi;
	pnew->height = h;
	pnew->open = 0;
	pnew->space[0] = 0.0;
	pnew->space[1] = 0.0;
	pnew->space[2] = 0.0;
	pnew->space[3] = 0.0;
}


double total_height(struct frame *fr)
{
	int i;
	double t = 0.0;
	for ( i=0; i<fr->n_paras; i++ ) {
		t += fr->paras[i]->height;
	}
	return t;
}


Paragraph *last_open_para(struct frame *fr)
{
	Paragraph *pnew;

	if ( (fr->paras != NULL) && (fr->paras[fr->n_paras-1]->open) ) {
		return fr->paras[fr->n_paras-1];
	}

	/* No open paragraph found, create a new one */
	pnew = create_paragraph(fr);
	if ( pnew == NULL ) return NULL;

	pnew->type = PARA_TYPE_TEXT;
	pnew->open = 1;
	pnew->n_runs = 0;
	pnew->runs = NULL;
	pnew->layout = NULL;
	pnew->height = 0.0;
	pnew->offset_last = 0;

	return pnew;
}


void close_last_paragraph(struct frame *fr)
{
	if ( fr->paras == NULL ) return;
	if ( fr->paras[fr->n_paras-1]->type != PARA_TYPE_TEXT ) {
		printf("Closing a non-text paragraph!\n");
	}
	fr->paras[fr->n_paras-1]->open = 0;
}


int last_para_available_for_text(struct frame *fr)
{
	Paragraph *last_para;
	if ( fr->paras == NULL ) return 0;
	last_para = fr->paras[fr->n_paras-1];
	if ( last_para->type == PARA_TYPE_TEXT ) {
		if ( last_para->open ) return 1;
	}
	return 0;
}


static void render_from_surf(cairo_surface_t *surf, cairo_t *cr,
                             double w, double h, int border)
{
	double x, y;
	int sw, sh;

	x = 0.0;  y = 0.0;
	cairo_user_to_device(cr, &x, &y);
	x = rint(x);  y = rint(y);
	cairo_device_to_user(cr, &x, &y);

	sw = cairo_image_surface_get_width(surf);
	sh = cairo_image_surface_get_height(surf);

	cairo_save(cr);
	cairo_scale(cr, w/sw, h/sh);
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, sw, sh);
	cairo_set_source_surface(cr, surf, 0.0, 0.0);
	cairo_pattern_t *patt = cairo_get_source(cr);
	cairo_pattern_set_extend(patt, CAIRO_EXTEND_PAD);
	cairo_pattern_set_filter(patt, CAIRO_FILTER_BEST);
	cairo_fill(cr);
	cairo_restore(cr);

	if ( border ) {
		cairo_new_path(cr);
		cairo_rectangle(cr, x+0.5, y+0.5, w, h);
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);
	}
}


void render_paragraph(cairo_t *cr, Paragraph *para, ImageStore *is)
{
	cairo_surface_t *surf;
	cairo_surface_type_t type;
	double w, h;

	cairo_translate(cr, para->space[0], para->space[2]);

	type = cairo_surface_get_type(cairo_get_target(cr));

	switch ( para->type ) {

		case PARA_TYPE_TEXT :
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
		pango_cairo_update_layout(cr, para->layout);
		pango_cairo_show_layout(cr, para->layout);
		cairo_fill(cr);
		break;

		case PARA_TYPE_IMAGE :
		w = para->image_w;
		h = para->image_h;
		cairo_user_to_device_distance(cr, &w, &h);
		surf = lookup_image(is, para->filename, w);
		if ( surf != NULL ) {
			render_from_surf(surf, cr, para->image_w, para->image_h, 0);
		} else {
			printf("surf = NULL!\n");
		}
		break;

		case PARA_TYPE_CALLBACK :
		w = para->cb_w;
		h = para->cb_h;
		cairo_user_to_device_distance(cr, &w, &h);
		if ( type == CAIRO_SURFACE_TYPE_PDF ) {
			w *= 6;  h *= 6;
		}
		surf = para->draw_func(w, h,
		                       para->bvp, para->vp);
		render_from_surf(surf, cr, para->cb_w, para->cb_h, 1);
		cairo_surface_destroy(surf);  /* FIXME: Cache like crazy */
		break;

	}
}


size_t end_offset_of_para(struct frame *fr, int pn)
{
	int i;
	size_t total = 0;
	for ( i=0; i<fr->paras[pn]->n_runs; i++ ) {
		total += fr->paras[pn]->runs[i].len_bytes;
	}
	return total;
}


/* Local x,y in paragraph -> text offset */
static size_t text_para_pos(Paragraph *para, double x, double y, int *ptrail)
{
	int idx;
	pango_layout_xy_to_index(para->layout, pango_units_from_double(x),
	                         pango_units_from_double(y), &idx, ptrail);
	return idx;
}


void show_edit_pos(struct edit_pos a)
{
	printf("para %i, pos %li, trail %i\n", a.para, (long int)a.pos, a.trail);
}


int positions_equal(struct edit_pos a, struct edit_pos b)
{
	if ( a.para != b.para ) return 0;
	if ( a.pos != b.pos ) return 0;
	if ( a.trail != b.trail ) return 0;
	return 1;
}


void sort_positions(struct edit_pos *a, struct edit_pos *b)
{
	if ( a->para > b->para ) {
		size_t tpos;
		int tpara, ttrail;
		tpara = b->para;   tpos = b->pos;  ttrail = b->trail;
		b->para = a->para;  b->pos = a->pos;  b->trail = a->trail;
		a->para = tpara;    a->pos = tpos;    a->trail = ttrail;
	}

	if ( (a->para == b->para) && (a->pos > b->pos) )
	{
		size_t tpos = b->pos;
		int ttrail = b->trail;
		b->pos = a->pos;  b->trail = a->trail;
		a->pos = tpos;    a->trail = ttrail;
	}
}


int find_cursor_2(struct frame *fr, double x, double y,
                  struct edit_pos *pos)
{
	double pad = fr->pad_t;
	int i;

	if ( fr == NULL ) {
		fprintf(stderr, "Cursor frame is NULL.\n");
		return 1;
	}

	for ( i=0; i<fr->n_paras; i++ ) {
		double npos = pad + fr->paras[i]->height;
		if ( npos > y ) {
			pos->para = i;
			if ( fr->paras[i]->type == PARA_TYPE_TEXT ) {
				pos->pos = text_para_pos(fr->paras[i],
				          x - fr->pad_l - fr->paras[i]->space[0],
				          y - pad - fr->paras[i]->space[2],
				          &pos->trail);
			} else {
				pos->pos = 0;
			}
			return 0;
		}
		pad = npos;
	}

	if ( fr->n_paras == 0 ) {
		printf("No paragraphs in frame.\n");
		return 1;
	}

	/* Pretend it's in the last paragraph */
	pad -= fr->paras[fr->n_paras-1]->height;
	pos->para = fr->n_paras - 1;
	pos->pos = text_para_pos(fr->paras[fr->n_paras-1],
	                         x - fr->pad_l, y - pad, &pos->trail);
	return 0;
}


int find_cursor(struct frame *fr, double x, double y,
                int *ppara, size_t *ppos, int *ptrail)
{
	struct edit_pos p;
	int r;
	r = find_cursor_2(fr, x, y, &p);
	if ( r ) return r;
	*ppara = p.para;
	*ppos = p.pos;
	*ptrail = p.trail;
	return 0;
}


int get_para_highlight(struct frame *fr, int cursor_para,
                       double *cx, double *cy, double *cw, double *ch)
{
	Paragraph *para;
	int i;
	double py = 0.0;

	if ( fr == NULL ) {
		fprintf(stderr, "Cursor frame is NULL.\n");
		return 1;
	}

	if ( cursor_para >= fr->n_paras ) {
		fprintf(stderr, "Highlight paragraph number is too high!\n");
		return 1;
	}

	para = fr->paras[cursor_para];
	for ( i=0; i<cursor_para; i++ ) {
		py += fr->paras[i]->height;
	}

	*cx = fr->pad_l + para->space[0];
	*cy = fr->pad_t + py + para->space[2];
	*cw = fr->w - fr->pad_l - fr->pad_r - para->space[0] - para->space[1];
	*ch = para->height - para->space[2] - para->space[3];
	return 0;
}


int get_cursor_pos(struct frame *fr, int cursor_para, int cursor_pos,
                   double *cx, double *cy, double *ch)
{
	Paragraph *para;
	PangoRectangle rect;
	int i;
	double py = 0.0;

	if ( fr == NULL ) {
		fprintf(stderr, "Cursor frame is NULL.\n");
		return 1;
	}

	if ( cursor_para >= fr->n_paras ) {
		fprintf(stderr, "Cursor paragraph number is too high!\n");
		return 1;
	}

	para = fr->paras[cursor_para];
	for ( i=0; i<cursor_para; i++ ) {
		py += fr->paras[i]->height;
	}

	if ( para->type != PARA_TYPE_TEXT ) {
		return 1;
	}

	pango_layout_get_cursor_pos(para->layout, cursor_pos, &rect, NULL);

	*cx = pango_units_to_double(rect.x) + fr->pad_l + para->space[0];
	*cy = pango_units_to_double(rect.y) + fr->pad_t + py + para->space[2];
	*ch = pango_units_to_double(rect.height);
	return 0;
}


void cursor_moveh(struct frame *fr, int *cpara, size_t *cpos, int *ctrail,
                  signed int dir)
{
	Paragraph *para = fr->paras[*cpara];
	int np = *cpos;

	pango_layout_move_cursor_visually(para->layout, 1, *cpos, *ctrail,
	                                  dir, &np, ctrail);
	if ( np == -1 ) {
		if ( *cpara > 0 ) {
			size_t end_offs;
			(*cpara)--;
			end_offs = end_offset_of_para(fr, *cpara);
			if ( end_offs > 0 ) {
				*cpos = end_offs - 1;
				*ctrail = 1;
			} else {
				/* Jumping into an empty paragraph */
				*cpos = 0;
				*ctrail = 0;
			}
			return;
		} else {
			/* Can't move any further */
			return;
		}
	}

	if ( np == G_MAXINT ) {
		if ( *cpara < fr->n_paras-1 ) {
			(*cpara)++;
			*cpos = 0;
			*ctrail = 0;
			return;
		} else {
			/* Can't move any further */
			return;
		}
	}

	*cpos = np;
}


void cursor_movev(struct frame *fr, int *cpara, size_t *cpos, int *ctrail,
                  signed int dir)
{
}


void check_callback_click(struct frame *fr, int para)
{
	Paragraph *p = fr->paras[para];
	if ( p->type == PARA_TYPE_CALLBACK ) {
		p->click_func(0.0, 0.0, p->bvp, p->vp);
	}
}


static int which_run(Paragraph *para, size_t offs)
{
	int i;

	for ( i=0; i<para->n_runs; i++ ) {
		struct text_run *run = &para->runs[i];
		if ( (offs >= run->para_offs_bytes)
		  && (offs <= run->para_offs_bytes + run->len_bytes) )
		{
			return i;
		}
	}
	return para->n_runs;
}


size_t pos_trail_to_offset(Paragraph *para, size_t offs, int trail)
{
	glong char_offs;
	size_t run_offs;
	const char *run_text;
	struct text_run *run;
	int nrun;
	char *ptr;

	nrun = which_run(para, offs);

	if ( nrun == para->n_runs ) {
		fprintf(stderr, "pos_trail_to_offset: Offset too high\n");
		return 0;
	}

	run = &para->runs[nrun];

	if ( para->type != PARA_TYPE_TEXT ) return 0;

	if ( run == NULL ) {
		fprintf(stderr, "pos_trail_to_offset: No run\n");
		return 0;
	}

	if ( run->scblock == NULL ) {
		fprintf(stderr, "pos_trail_to_offset: SCBlock = NULL?\n");
		return 0;
	}

	if ( (sc_block_name(run->rscblock) != NULL)
	  && (strcmp(sc_block_name(run->rscblock), "newpara") == 0) )
	{
		return 0;
	}

	if ( sc_block_contents(run->rscblock) == NULL ) {
		fprintf(stderr, "pos_trail_to_offset: No contents "
		        "(%p name=%s, options=%s)\n",
		        run->scblock, sc_block_name(run->scblock),
		        sc_block_options(run->scblock));
		return 0;
	}

	/* Get the text for the run */
	run_text = sc_block_contents(run->rscblock);

	/* Turn  the paragraph offset into a run offset */
	run_offs = offs - run->para_offs_bytes;

	char_offs = g_utf8_pointer_to_offset(run_text, run_text+run_offs);
	char_offs += trail;

	if ( char_offs > g_utf8_strlen(run_text, -1) ) {
		printf("Offset outside string! '%s'\n"
		       "char_offs %li offs %li len %li\n",
		       run_text, (long int)char_offs, (long int)offs,
		       (long int)g_utf8_strlen(run_text, -1));
	}

	ptr = g_utf8_offset_to_pointer(run_text, char_offs);
	return ptr - run_text + run->para_offs_bytes;
}


void insert_text_in_paragraph(Paragraph *para, size_t offs, const char *t)
{
	int nrun;
	int i;
	struct text_run *run;
	size_t run_offs, ins_len;

	/* Find which run we are in */
	nrun = which_run(para, offs);
	if ( nrun == para->n_runs ) {
		fprintf(stderr, "Couldn't find run to insert into.\n");
		return;
	}
	run = &para->runs[nrun];

	if ( (sc_block_name(run->scblock) != NULL)
	  && (strcmp(sc_block_name(run->scblock), "newpara") == 0) )
	{

		SCBlock *nnp;
		printf("Inserting into newpara block...\n");

		/* Add a new \newpara block after this one */
		nnp = sc_block_append(run->scblock, "newpara",
		                      NULL, NULL, NULL);

		/* The first \newpara block becomes a normal anonymous block */
		sc_block_set_name(run->scblock, NULL);

		if ( para->newline_at_end == run->scblock ) {
			para->newline_at_end = nnp;
			printf("Replaced the newpara block\n");
		}

	}

	/* Translate paragraph offset for insertion into SCBlock offset */
	run_offs = offs - run->para_offs_bytes;
	sc_insert_text(run->rscblock, run_offs, t);

	/* Update length of this run */
	ins_len = strlen(t);
	run->len_bytes += ins_len;

	/* Update offsets of subsequent runs */
	for ( i=nrun+1; i<para->n_runs; i++ ) {
		para->runs[i].para_offs_bytes += ins_len;
	}
}


static SCBlock *pos_to_rscblock(struct frame *fr, struct edit_pos p)
{
	int run;
	size_t paraoffs;
	Paragraph *para;

	para = fr->paras[p.para];

	if ( para->type != PARA_TYPE_TEXT ) {
		return NULL;
	}

	paraoffs = pos_trail_to_offset(para, p.pos, p.trail);

	run = which_run(para, paraoffs);
	assert(run < para->n_runs);

	return para->runs[run].rscblock;
}



static SCBlock *pos_to_scblock(struct frame *fr, struct edit_pos p,
                               enum para_type *type)
{
	int run;
	size_t paraoffs;
	Paragraph *para;

	para = fr->paras[p.para];
	if ( type != NULL ) {
		*type = para->type;
	}

	if ( para->type != PARA_TYPE_TEXT ) {
		return para->scblock;
	}

	paraoffs = pos_trail_to_offset(para, p.pos, p.trail);

	run = which_run(para, paraoffs);
	assert(run < para->n_runs);

	return para->runs[run].scblock;
}


static size_t pos_to_offset(struct frame *fr, struct edit_pos p)
{
	int run;
	size_t paraoffs;
	Paragraph *para;

	para = fr->paras[p.para];
	if ( para->type != PARA_TYPE_TEXT ) {
		return 0;
	}

	paraoffs = pos_trail_to_offset(para, p.pos, p.trail);

	run = which_run(para, paraoffs);
	assert(run < para->n_runs);

	return paraoffs - para->runs[run].para_offs_bytes;
}


static int pos_to_run_number(struct frame *fr, struct edit_pos p)
{
	int run;
	size_t paraoffs;
	Paragraph *para;

	para = fr->paras[p.para];
	if ( para->type != PARA_TYPE_TEXT ) {
		return 0;
	}

	paraoffs = pos_trail_to_offset(para, p.pos, p.trail);

	run = which_run(para, paraoffs);
	assert(run < para->n_runs);

	return run;
}


/* Reduce para_offs_bytes for all runs in "para" after "start" by "del" */
static void update_subsq_para_offsets(Paragraph *para, int start, size_t del)
{
	int i;

	for ( i=start+1; i<para->n_runs; i++ ) {
		para->runs[i].para_offs_bytes -= del;
	}
}


static size_t delete_run(Paragraph *para, int nrun)
{
	size_t del;

	printf("deleting run %i of %i from para %p\n", nrun, para->n_runs, para);
	del = para->runs[nrun].len_bytes;
	memmove(&para->runs[nrun], &para->runs[nrun+1],
	        (para->n_runs-1)*sizeof(struct text_run));
	para->n_runs--;

	return del;
}


static Paragraph *scan_runs_for_scblock(struct frame *fr, int pn1, int pn2,
                                        SCBlock *bl, int *run)
{
	int i;
	for ( i=pn1; i<=pn2; i++ ) {
		int j;
		for ( j=0; j<fr->paras[i]->n_runs; j++ ) {
			if ( fr->paras[i]->runs[j].rscblock == bl ) {
				*run = j;
				return fr->paras[i];
			}
		}
	}
	return NULL;
}


static Paragraph *find_run_for_scblock_next(struct frame *fr, int pn1, int pn2,
                                            SCBlock *bl, int *run)
{
	if ( sc_block_child(bl) != NULL ) {
		Paragraph *para;
		para = find_run_for_scblock_next(fr, pn1, pn2,
		                                 sc_block_child(bl), run);
		if ( para != NULL ) return para;
	}

	do {
		Paragraph *para;
		para = scan_runs_for_scblock(fr, pn1, pn2, bl, run);
		if ( para != NULL ) return para;
		bl = sc_block_next(bl);
	} while ( bl != NULL );

	return NULL;
}


/* Find the run which contains the text from "bl",
 * taking into account that it might be a child block, for example:
 *   {some text}
 *   \italic          <---- bl points here
 *      {more text}   <---- but this block is referenced by the run
 *   {final text}
 */
static Paragraph *find_run_for_scblock(struct frame *fr, int pn1, int pn2,
                                       SCBlock *bl, int *run)
{
	Paragraph *para;

	show_sc_block(bl, "searching ");
	para = scan_runs_for_scblock(fr, pn1, pn2, bl, run);
	if ( para != NULL ) return para;

	if ( sc_block_child(bl) != NULL ) {
		para = find_run_for_scblock_next(fr, pn1, pn2, sc_block_child(bl), run);
		if ( para != NULL ) return para;
	}

	return NULL;
}


static int paragraph_number(struct frame *fr, Paragraph *p, int *err)
{
	int i;
	for ( i=0; i<fr->n_paras; i++ ) {
		if ( fr->paras[i] == p ) return i;
	}
	fprintf(stderr, "Couldn't find paragraph %p\n", p);
	*err = 1;
	return 0;
}


static size_t delete_run_for_scblock(struct frame *fr,
                                     Paragraph *p1, Paragraph *p2, SCBlock *bl)
{
	int pn1, pn2;
	int err = 0;
	size_t del;
	Paragraph *para;
	int run;

	pn1 = paragraph_number(fr, p1, &err);
	pn2 = paragraph_number(fr, p2, &err);
	if ( err ) return 0;

	para = find_run_for_scblock(fr, pn1, pn2, bl, &run);
	if ( para == NULL ) {
		fprintf(stderr, "Couldn't find block %p between paragraphs %p and %p\n",
		        bl, p1, p2);
		return 0;
	}

	del = delete_run(para, run);
	update_subsq_para_offsets(para, run, del);

	return del;
}


static signed int merge_paragraph_runs(Paragraph *p1, Paragraph *p2)
{
	struct text_run *runs_new;
	int i, spos;

	/* All the runs from p2 get added to p1 */
	runs_new = realloc(p1->runs,
	                   (p1->n_runs+p2->n_runs)*sizeof(struct text_run));
	if ( runs_new == NULL ) {
		fprintf(stderr, "Failed to allocate merged runs.\n");
		return -1;
	}
	p1->runs = runs_new;

	assert(p1->runs[p1->n_runs-1].scblock == get_newline_at_end(p1));
	p1->n_runs--;  /* Chop off the run corresponding to \newpara */
	spos = p1->n_runs;

	/* The end of the united paragraph should now be the end of the
	 * second one */
	set_newline_at_end(p1, get_newline_at_end(p2));

	for ( i=0; i<p2->n_runs; i++ ) {

		size_t offs;

		p1->runs[p1->n_runs] = p2->runs[i];

		offs = p1->runs[p1->n_runs-1].para_offs_bytes;
		offs += p1->runs[p1->n_runs-1].len_bytes;
		p1->runs[p1->n_runs].para_offs_bytes = offs;

		p1->n_runs++;

	}
	free(p2->runs);
	free(p2);

	return spos;
}


void merge_paragraphs(struct frame *fr, int para)
{
	Paragraph *p1, *p2;
	int i;
	SCBlock *n;

	if ( para >= fr->n_paras-1 ) {
		printf("Paragraph number too high to merge.\n");
		return;
	}

	p1 = fr->paras[para];
	p2 = fr->paras[para+1];

	if ( (p1->type != PARA_TYPE_TEXT) || (p2->type != PARA_TYPE_TEXT) ) {
		printf("Trying to merge non-text paragraphs.\n");
		return;
	}

	/* Delete the \newpara block to unite the paragraphs */
	n = get_newline_at_end(p1);
	assert(n != NULL);

	if ( sc_block_delete(&fr->scblocks, n) ) {
		fprintf(stderr, "Failed to delete paragraph end sentinel.\n");
		return;
	}

	merge_paragraph_runs(p1, p2);

	for ( i=para+1; i<fr->n_paras-1; i++ ) {
		fr->paras[i] = fr->paras[i+1];
	}
	fr->n_paras--;
}



static void merge_paragraphs_by_newpara(struct frame *fr, SCBlock *np)
{
	int i;
	Paragraph *p1;
	Paragraph *p2;

	for ( i=0; i<fr->n_paras-1; i++ ) {
		if ( fr->paras[i]->newline_at_end == np ) {

			int j;
			signed int spos;

			p1 = fr->paras[i];
			p2 = fr->paras[i+1];

			printf("-------------------------------\n");
			show_para(p1);
			printf("---x--------x------------------\n");
			show_para(p2);
			spos = merge_paragraph_runs(p1, p2);
			if ( spos < 0 ) {
				fprintf(stderr, "Failed to merge paragraphs\n");
				return;
			}
			printf("-------------------------------\n");
			show_para(p1);

			for ( j=i+1; j<fr->n_paras-1; j++ ) {
				fr->paras[j] = fr->paras[j+1];
			}
			fr->n_paras--;

			return;

		}
	}

	fprintf(stderr, "Couldn't find paragraphs to merge by newpara\n");
}


static int find_block_inside(SCBlock *needle, SCBlock *bl)
{
	if ( needle == bl ) return 1;

	if ( sc_block_child(bl) != NULL ) {
		if ( find_block_inside(needle, sc_block_child(bl)) ) return 1;
	}

	if ( sc_block_next(bl) != NULL ) {
		if ( find_block_inside(needle, sc_block_next(bl)) ) return 1;
	}

	return 0;
}


/* Return true if "top" either IS "child", or contains "child" somewhere
 * underneath, even if via a macro expansion */
static int block_is_under(SCBlock *needle, SCBlock *top)
{
	if ( needle == top ) return 1;

	if ( sc_block_child(top) != NULL ) {
		if ( find_block_inside(needle, sc_block_child(top)) ) return 1;
	}

	/* Do not look at top->next here */

	return 0;
}


void delete_text_from_frame(struct frame *fr, struct edit_pos p1, struct edit_pos p2,
                            double wrapw)
{
	int i;
	SCBlock *p1scblock, *p2scblock;
	SCBlock *p1rscblock, *p2rscblock;
	enum para_type type1, type2;
	SCBlock *scblock;

	sort_positions(&p1, &p2);

	printf("para %i offs %ld\n", p1.para, p1.pos);
	printf("para %i offs %li\n", p2.para, p2.pos);

	/* Find SC positions for start and end */
	p1scblock = pos_to_scblock(fr, p1, &type1);
	p2scblock = pos_to_scblock(fr, p2, &type2);
	p1rscblock = pos_to_rscblock(fr, p1);
	p2rscblock = pos_to_rscblock(fr, p2);

	printf("SCBlocks %p to %p\n", p1scblock, p2scblock);
	//show_sc_blocks(p1scblock);

	if ( (p1scblock == p2scblock) && (type1 == PARA_TYPE_TEXT) ) {

		size_t del;
		int p1run;
		size_t p1offs, p2offs;
		printf("Simple case, one SCBlock\n");

		assert(type1 == type2);

		/* Remove the text and update the run length */
		p1offs = pos_to_offset(fr, p1);
		p2offs = pos_to_offset(fr, p2);
		del = scblock_delete_text(p1scblock, p1offs, p2offs);

		p1run = pos_to_run_number(fr, p1);
		fr->paras[p1.para]->runs[p1run].len_bytes -= del;

		/* Update the paragraph offsets of subsequent runs */
		update_subsq_para_offsets(fr->paras[p1.para], p1run, del);

		wrap_paragraph(fr->paras[p1.para], NULL, wrapw, 0, 0);

		return;
	}

	/* Starting point for iteration over blocks in middle of range.
	 * Record this now, because p1scblock might be about to get deleted */
	scblock = sc_block_next(p1scblock);

	/* First SCBlock in range: delete whole thing or second half */
	printf("First block %p\n", p1scblock);
	if ( type1 == PARA_TYPE_TEXT ) {

		size_t p1offs = pos_to_offset(fr, p1);
		int p1run = pos_to_run_number(fr, p1);
		printf("  offs %li\n", (long int)p1offs);
		if ( p1offs != 0 ) {
			size_t del;
			printf("Partial delete\n");
			printf("contents '%s'\n", sc_block_contents(p1rscblock));
			printf("from offs %li\n", (long int)p1offs);
			del = scblock_delete_text(p1rscblock, p1offs, -1);
			fr->paras[p1.para]->runs[p1run].len_bytes -= del;
			update_subsq_para_offsets(fr->paras[p1.para], p1run, del);
		} else {
			printf("Deleting the whole text SCBlock\n");
			sc_block_delete(&fr->scblocks, p1scblock);
			delete_run(fr->paras[p1.para], p1run);
		}

	} else {
		printf("Deleting the whole non-text SCBlock\n");
		sc_block_delete(&fr->scblocks, p1scblock);
	}

	/* Delete all the complete SCBlocks in the middle of the range */
	if ( !block_is_under(p2scblock, scblock) ) {
		do {

			SCBlock *next;

			/* For each SC block in middle of range: */
			printf("Deleting %p\n", scblock);
			if ( scblock == NULL ) {
				fprintf(stderr, "nothing?\n");
				break;
			}
			printf("name is '%s'\n", sc_block_name(scblock));
			if ( (sc_block_name(scblock) != NULL)
			  && (strcmp(sc_block_name(scblock), "newpara") == 0) )
			{
				/* Deleting newpara block, merge the paragraphs */
				merge_paragraphs_by_newpara(fr, scblock);

			}

			next = sc_block_next(scblock);
			delete_run_for_scblock(fr, fr->paras[p1.para],
			                       fr->paras[p2.para], scblock);
			sc_block_delete(&fr->scblocks, scblock);

			scblock = next;

		} while ( !block_is_under(p2scblock, scblock) );
	}

	/* Last SCBlock in range: delete whole thing or first half */
	printf("Last block %p (%s)\n", p2scblock, sc_block_name(p2scblock));
	if ( type2 == PARA_TYPE_TEXT ) {
		size_t len;
		size_t p2offs = pos_to_offset(fr, p2);
		int p2run = pos_to_run_number(fr, p2);
		printf("  offs %li\n", (long int)p2offs);
		if ( sc_block_contents(p2rscblock) != NULL ) {
			len = strlen(sc_block_contents(p2rscblock));
		} else {
			len = 0;
		}
		printf("  len %li\n", (long int)len);
		if ( (len > 0) && (p2offs == len) ) {
			printf("Deleting the whole text SCBlock\n");
			printf("deleting block %p\n", p2scblock);
			show_sc_block(p2scblock, "");
			sc_block_delete(&fr->scblocks, p2scblock);
			delete_run(fr->paras[p2.para], p2run);
		} else if ( p2offs > 0 ) {
			size_t del;
			printf("Partial delete\n");
			printf("contents '%s'\n", sc_block_contents(p2rscblock));
			printf("up to offs %li\n", (long int)p2offs);
			del = scblock_delete_text(p2rscblock, 0, p2offs);
			fr->paras[p2.para]->runs[p2run].len_bytes -= del;
			update_subsq_para_offsets(fr->paras[p2.para], p2run, del);
		} /* else do nothing */
	} else {
		printf("Deleting the whole non-text SCBlock\n");
		sc_block_delete(&fr->scblocks, p2scblock);
	}

	/* If any paragraphs have been deleted, this will wrap too many
	 * paragraphs, but it doesn't matter as long as we don't wrap
	 * past the end of the frame's contents. */
	for ( i=p1.para; i<=p2.para; i++ ) {
		if ( i >= fr->n_paras ) break;
		printf("Wrapping para %i (%p)\n", i, fr->paras[i]);
		wrap_paragraph(fr->paras[i], NULL, wrapw, 0, 0);
	}
	printf("All done.\n");
}


/* offs2 negative means "to end"
 * offs1 and offs2 are byte offsets within paragraph */
size_t delete_text_in_paragraph(struct frame *fr, int npara, size_t offs1, ssize_t offs2)
{
	int nrun1, nrun2, nrun;
	int i;
	size_t scblock_offs1, scblock_offs2;
	size_t sum_del = 0;
	Paragraph *para = fr->paras[npara];

	/* Find which run we are in */
	nrun1 = which_run(para, offs1);
	if ( offs2 < 0 ) {
		/* Delete to end */
		nrun2 = para->n_runs-1;
	} else {
		nrun2 = which_run(para, offs2);
	}
	if ( (nrun1 == para->n_runs) || (nrun2 == para->n_runs) ) {
		fprintf(stderr, "Couldn't find run to delete from.\n");
		return 0;
	}

	for ( nrun=nrun1; nrun<=nrun2; nrun++ ) {

		ssize_t ds, de;
		struct text_run *run;

		run = &para->runs[nrun];

		ds = offs1 - run->para_offs_bytes;
		if ( offs2 < 0 ) {
			de = run->len_bytes;
		} else {
			de = offs2 - run->para_offs_bytes;
		}
		if ( ds < 0 ) ds = 0;
		if ( de > run->len_bytes ) {
			de = run->len_bytes;
		}
		assert(ds >= 0);  /* Otherwise nrun1 was too big */
		assert(de >= 0);  /* Otherwise nrun2 was too big */
		if ( ds == de ) continue;

		/* Delete from the corresponding SC block */
		scblock_offs1 = ds;
		scblock_offs2 = de;
		sum_del += scblock_offs2 - scblock_offs1;
		scblock_delete_text(run->scblock, scblock_offs1, scblock_offs2);

		/* Fix up the offsets of the subsequent text runs */
		size_t del_len = de - ds;
		run->len_bytes -= del_len;
		for ( i=nrun+1; i<para->n_runs; i++ ) {
			para->runs[i].para_offs_bytes -= del_len;
		}

		offs1 -= del_len;
		offs2 -= del_len;

	}

	return sum_del;
}


static char *run_text(struct text_run *run)
{
	return strndup(sc_block_contents(run->rscblock), run->len_bytes);
}


void show_para(Paragraph *p)
{
	int i;
	printf("Paragraph %p\n", p);

	if ( p->type == PARA_TYPE_TEXT ) {

		printf("%i runs:\n", p->n_runs);
		for ( i=0; i<p->n_runs; i++ ) {
			char *tmp = run_text(&p->runs[i]);
			printf("  Run %2i: para offs %lli, SCBlock %p, len "
			       "%lli %s '%s'\n",
			       i, (long long int)p->runs[i].para_offs_bytes,
			       p->runs[i].scblock,
			       (long long int)p->runs[i].len_bytes,
			       pango_font_description_to_string(p->runs[i].fontdesc),
			       tmp);
			free(tmp);
		}

	} else if ( p->type == PARA_TYPE_IMAGE ) {
		printf("  Image: %s\n", p->filename);
	} else {
		printf("  Other paragraph type\n");
	}
}


static void check_para(Paragraph *para)
{
	int i;
	for ( i=0; i<para->n_runs; i++ ) {
		const char *run_text;
		if ( sc_block_contents(para->runs[i].rscblock) == NULL ) continue;
		run_text = sc_block_contents(para->runs[i].rscblock);
		if ( strlen(run_text) < para->runs[i].len_bytes ) {
			printf("found a wrong run\n");
			printf("run %i, para offs %li text '%s'\n", i,
			       (long int)para->runs[i].para_offs_bytes,
			       run_text);
			printf("(the SCBlock contains '%s'\n", sc_block_contents(para->runs[i].scblock));
			printf("run %p block %p\n", &para->runs[i], para->runs[i].scblock);
		}
	}
}


static SCBlock *split_text_paragraph(struct frame *fr, int pn, size_t pos,
                                     PangoContext *pc)
{
	Paragraph *pnew;
	int i;
	size_t offs, run_offs;
	int run;
	Paragraph *para = fr->paras[pn];
	struct text_run *rr;
	int bf_pos;

	pnew = insert_paragraph(fr, pn);
	if ( pnew == NULL ) {
		fprintf(stderr, "Failed to insert paragraph\n");
		return NULL;
	}

	/* Determine which run the cursor is in */
	run = which_run(para, pos);

	pnew->type = PARA_TYPE_TEXT;
	pnew->open = para->open;
	pnew->n_runs = para->n_runs - run;
	pnew->runs = malloc(pnew->n_runs * sizeof(struct text_run));
	if ( pnew->runs == NULL ) {
		fprintf(stderr, "Failed to allocate runs.\n");
		return NULL; /* Badness is coming */
	}

	/* Copy spacing */
	for ( i=0; i<4; i++ ) pnew->space[i] = para->space[i];

	rr = &para->runs[run];
	run_offs = pos - rr->para_offs_bytes;
	printf("split at run %i\n", run);

	if ( rr->len_bytes == run_offs ) {

		/* We are splitting at a run boundary, so things are easy */

		if ( run == para->n_runs-1 ) {

			/* It's actually a paragraph boundary.  Even easier still... */
			printf("splitting at end of para\n");
			pnew->runs[0].para_offs_bytes = 0;
			pnew->runs[0].scblock = rr->scblock;
			pnew->runs[0].rscblock = rr->rscblock;
			pnew->runs[0].para_offs_bytes = 0;
			pnew->runs[0].len_bytes = 0;
			pnew->runs[0].fontdesc = pango_font_description_copy(rr->fontdesc);
			pnew->runs[0].col[0] = rr->col[0];
			pnew->runs[0].col[1] = rr->col[1];
			pnew->runs[0].col[2] = rr->col[2];
			pnew->runs[0].col[3] = rr->col[3];
			pnew->n_runs = 1;


		} else {

			pnew->runs[0] = para->runs[run+1];
			pnew->runs[0].para_offs_bytes = 0;
			pnew->n_runs = 1;

		}

		/* We start copying runs after the whole second one which we
		 * just brought forward, i.e. we start at the THIRD run */
		bf_pos = 2;

	} else {

		/* First run of the new paragraph contains the leftover text */
		pnew->runs[0].scblock = rr->scblock;
		pnew->runs[0].rscblock = rr->rscblock;
		pnew->runs[0].para_offs_bytes = 0;
		pnew->runs[0].len_bytes = rr->len_bytes - run_offs;
		printf("%i - %i\n", (int)rr->len_bytes, (int)run_offs);
		printf("second run len = %i\n", (int)pnew->runs[0].len_bytes);
		pnew->runs[0].fontdesc = pango_font_description_copy(rr->fontdesc);
		pnew->runs[0].col[0] = rr->col[0];
		pnew->runs[0].col[1] = rr->col[1];
		pnew->runs[0].col[2] = rr->col[2];
		pnew->runs[0].col[3] = rr->col[3];
		pnew->n_runs = 1;

		/* We start copying runs at the second run of the paragraph */
		bf_pos = 1;

	}

	/* All later runs just get moved to the new paragraph */
	offs = pnew->runs[0].len_bytes;
	for ( i=run+bf_pos; i<para->n_runs; i++ ) {
		pnew->runs[pnew->n_runs] = para->runs[i];
		pnew->runs[pnew->n_runs].para_offs_bytes = offs;
		pnew->n_runs++;
		offs += para->runs[i].len_bytes;
	}


	/* Truncate the first paragraph at the appropriate position */
	rr->len_bytes = run_offs;
	para->n_runs = run+1;

	/* If the first and second paragraphs have the same SCBlock, split it */
	if ( (rr->rscblock != NULL) && (rr->rscblock == pnew->runs[0].rscblock) ) {

		printf("splitting SCBlock at %i\n", (int)run_offs);
		printf("old block: '%s'\n", sc_block_contents(rr->rscblock));
		pnew->runs[0].rscblock = sc_block_split(rr->rscblock, run_offs);
		printf("new block 1: '%s'\n", sc_block_contents(rr->rscblock));
		printf("new block 2: '%s'\n", sc_block_contents(pnew->runs[0].rscblock));
		printf("run %p block %p\n", &pnew->runs[0], pnew->runs[0].rscblock);

		printf("run %i, para offs %li\n", 0,
		       (long int)pnew->runs[0].para_offs_bytes);
		printf("len %i\n", (int)pnew->runs[0].len_bytes);
	}

	/* Add a \newpara after the end of the first paragraph's SC */
	set_newline_at_end(para,
	                   sc_block_append(rr->scblock, strdup("newpara"),
	                                   NULL, NULL, NULL));

	pnew->open = para->open;
	para->open = 0;

	printf("first para:\n");
	check_para(para);
	printf("second para:\n");
	check_para(pnew);

	wrap_paragraph(para, pc, fr->w - fr->pad_l - fr->pad_r, 0, 0);
	wrap_paragraph(pnew, pc, fr->w - fr->pad_l - fr->pad_r, 0, 0);

	return sc_block_next(rr->scblock);
}


SCBlock *split_paragraph(struct frame *fr, int pn, size_t pos, PangoContext *pc)
{
	Paragraph *para = fr->paras[pn];

	if ( para->type == PARA_TYPE_TEXT ) {
		return split_text_paragraph(fr, pn, pos, pc);
	} else {
		/* Other types can't be split */
		return NULL;
	}
}


SCBlock *block_at_cursor(struct frame *fr, int pn, size_t pos)
{
	Paragraph *para = fr->paras[pn];

	if ( para->type != PARA_TYPE_CALLBACK ) return NULL;

	return para->scblock;
}


int get_sc_pos(struct frame *fr, int pn, size_t pos,
               SCBlock **bl, size_t *ppos)
{
	Paragraph *para = fr->paras[pn];
	int nrun;
	struct text_run *run;

	nrun = which_run(para, pos);
	if ( nrun == para->n_runs ) {
		fprintf(stderr, "Couldn't find run to insert into.\n");
		return 1;
	}
	run = &para->runs[nrun];

	*ppos = pos - run->para_offs_bytes;
	*bl = run->scblock;

	return 0;
}


void set_para_spacing(Paragraph *para, float space[4])
{
	if ( para == NULL ) return;
	para->space[0] = space[0];
	para->space[1] = space[1];
	para->space[2] = space[2];
	para->space[3] = space[3];
}


Paragraph *current_para(struct frame *fr)
{
	if ( fr == NULL ) return NULL;

	if ( (fr->paras != NULL) && (fr->paras[fr->n_paras-1]->open) ) {
		return fr->paras[fr->n_paras-1];
	}

	return NULL;
}

void *get_para_bvp(Paragraph *para)
{
	if ( para->type != PARA_TYPE_CALLBACK ) return NULL;
	return para->bvp;
}


SCBlock *para_scblock(Paragraph *para)
{
	return para->scblock;
}


enum para_type para_type(Paragraph *para)
{
	return para->type;
}

int para_debug_num_runs(Paragraph *para)
{
	if ( para->type != PARA_TYPE_TEXT ) return 0;
	return para->n_runs;
}

int para_debug_run_info(Paragraph *para, int i, size_t *len, SCBlock **scblock,
                        size_t *para_offs)
{
	if ( para->type != PARA_TYPE_TEXT ) return 1;
	if ( i >= para->n_runs ) return 1;

	*len = para->runs[i].len_bytes;
	*para_offs = para->runs[i].para_offs_bytes;
	*scblock = para->runs[i].scblock;
	return 0;
}
