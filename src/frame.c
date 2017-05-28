/*
 * frame.c
 *
 * Copyright © 2013-2017 Thomas White <taw@bitwiz.org.uk>
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

#include "sc_parse.h"
#include "frame.h"
#include "imagestore.h"


struct text_run
{
	SCBlock              *scblock;
	SCBlock              *macro_real_block;
	size_t                scblock_offs_bytes;  /* Offset from start of SCBlock */
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

	/* For PARA_TYPE_TEXT */
	int              n_runs;
	struct text_run *runs;
	int              open;
	PangoLayout     *layout;
	size_t           offset_last;

	/* For anything other than PARA_TYPE_TEXT
	 * (for text paragraphs, these things are in the runs) */
	SCBlock         *scblock;
	SCBlock         *macro_real_scblock;

	/* For PARA_TYPE_IMAGE */
	char            *filename;
	double           image_w;
	double           image_h;

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

		run_text = sc_block_contents(para->runs[i].scblock)
		           + para->runs[i].scblock_offs_bytes;

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


void add_run(Paragraph *para, SCBlock *scblock, SCBlock *macro_real,
             size_t offs_bytes, size_t len_bytes, PangoFontDescription *fdesc,
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
	para->runs[para->n_runs].macro_real_block = macro_real;
	para->runs[para->n_runs].scblock_offs_bytes = offs_bytes;
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
static Paragraph *insert_paragraph(struct frame *fr, int pos)
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

	fr->paras = paras_new;
	fr->n_paras ++;

	for ( i=fr->n_paras-1; i>pos; i-- ) {
		fr->paras[i] = fr->paras[i-1];
	}
	fr->paras[pos+1] = pnew;

	return pnew;
}


void add_callback_para(struct frame *fr, SCBlock *bl, SCBlock *mr,
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
	pnew->macro_real_scblock = mr;
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
                    double w, double h, int editable)
{
	Paragraph *pnew;

	pnew = create_paragraph(fr);
	if ( pnew == NULL ) {
		fprintf(stderr, "Failed to add image paragraph\n");
		return;
	}

	pnew->type = PARA_TYPE_IMAGE;
	pnew->scblock = scblock;
	pnew->filename = strdup(filename);
	pnew->image_w = w;
	pnew->image_h = h;
	pnew->height = h;
	pnew->open = 0;
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
	fr->paras[fr->n_paras-1]->open = 0;
}


static void render_from_surf(cairo_surface_t *surf, cairo_t *cr,
                             double w, double h, int border)
{
	double x, y;

	x = 0.0;  y = 0.0;
	cairo_user_to_device(cr, &x, &y);
	x = rint(x);  y = rint(y);
	cairo_device_to_user(cr, &x, &y);

	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, w, h);
	cairo_set_source_surface(cr, surf, 0.0, 0.0);
	cairo_fill(cr);

	if ( border ) {
		cairo_new_path(cr);
		cairo_rectangle(cr, x+0.5, y+0.5, w, h);
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);
	}
}


void render_paragraph(cairo_t *cr, Paragraph *para, ImageStore *is,
                      enum is_size isz)
{
	cairo_surface_t *surf;

	cairo_translate(cr, para->space[0], para->space[2]);

	switch ( para->type ) {

		case PARA_TYPE_TEXT :
		cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
		pango_cairo_update_layout(cr, para->layout);
		pango_cairo_show_layout(cr, para->layout);
		cairo_fill(cr);
		break;

		case PARA_TYPE_IMAGE :
		surf = lookup_image(is, para->filename, para->image_w, isz);
		render_from_surf(surf, cr, para->image_w, para->image_h, 0);
		break;

		case PARA_TYPE_CALLBACK :
		surf = para->draw_func(para->cb_w, para->cb_h,
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

	*cx = fr->pad_l;
	*cy = fr->pad_t + py;
	*cw = fr->w - fr->pad_l - fr->pad_r;
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
			(*cpara)--;
			*cpos = end_offset_of_para(fr, *cpara) - 1;
			*ctrail = 1;
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
	run = &para->runs[nrun];

	if ( run == NULL ) {
		fprintf(stderr, "pos_trail_to_offset: No run\n");
		return 0;
	}

	/* Get the text for the run */
	run_text = sc_block_contents(run->scblock) + run->scblock_offs_bytes;

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
	size_t run_offs, scblock_offs, ins_len;

	/* Find which run we are in */
	nrun = which_run(para, offs);
	if ( nrun == para->n_runs ) {
		fprintf(stderr, "Couldn't find run to insert into.\n");
		return;
	}
	run = &para->runs[nrun];

	if ( run->macro_real_block != NULL ) {
		printf("Not inserting text into a macro block.\n");
		return;
	}

	/* Translate paragraph offset for insertion into SCBlock offset */
	run_offs = offs - run->para_offs_bytes;
	scblock_offs = run_offs + run->scblock_offs_bytes;
	sc_insert_text(run->scblock, scblock_offs, t);

	/* Update length of this run */
	ins_len = strlen(t);
	run->len_bytes += ins_len;

	/* Update offsets of subsequent runs */
	for ( i=nrun+1; i<para->n_runs; i++ ) {
		if ( para->runs[i].scblock == run->scblock ) {
			para->runs[i].scblock_offs_bytes += ins_len;
		}
		para->runs[i].para_offs_bytes += ins_len;
	}
}


static void delete_paragraph(struct frame *fr, int p)
{
}


void delete_text_from_frame(struct frame *fr, struct edit_pos p1, struct edit_pos p2,
                            double wrapw)
{
	int i;

	sort_positions(&p1, &p2);

	for ( i=p1.para; i<=p2.para; i++ ) {

		size_t start;
		ssize_t finis;

		Paragraph *para = fr->paras[i];

		if ( i == p1.para ) {
			start = pos_trail_to_offset(para, p1.pos, p1.trail);
		} else {
			start = 0;
		}

		if ( i == p2.para ) {
			finis = pos_trail_to_offset(para, p2.pos, p2.trail);
		} else {
			finis = -1;
		}

		if ( (start == 0) && (finis == -1) ) {
			delete_paragraph(fr, i);
		} else {
			delete_text_in_paragraph(para, start, finis);
			wrap_paragraph(para, NULL, wrapw, 0, 0);
		}

	}
}


/* offs2 negative means "to end" */
void delete_text_in_paragraph(Paragraph *para, size_t offs1, ssize_t offs2)
{
	int nrun1, nrun2, nrun;
	int i;
	size_t scblock_offs1, scblock_offs2;

	/* Find which run we are in */
	nrun1 = which_run(para, offs1);
	nrun2 = which_run(para, offs2);
	if ( (nrun1 == para->n_runs) || (nrun2 == para->n_runs) ) {
		fprintf(stderr, "Couldn't find run to delete from.\n");
		return;
	}

	for ( nrun=nrun1; nrun<=nrun2; nrun++ ) {

		ssize_t ds, de;
		struct text_run *run;

		run = &para->runs[nrun];

		if ( run->macro_real_block != NULL ) {
			printf("Not deleting text from macro block\n");
			continue;
		}

		ds = offs1 - run->para_offs_bytes;
		de = offs2 - run->para_offs_bytes;
		if ( ds < 0 ) ds = 0;
		if ( de > run->len_bytes ) {
			de = run->len_bytes;
		}
		assert(ds >= 0);  /* Otherwise nrun1 was too big */
		assert(de >= 0);  /* Otherwise nrun2 was too big */
		if ( ds == de ) continue;

		/* Delete from the corresponding SC block */
		scblock_offs1 = ds + run->scblock_offs_bytes;
		scblock_offs2 = de + run->scblock_offs_bytes;
		scblock_delete_text(run->scblock, scblock_offs1, scblock_offs2);

		/* Fix up the offsets of the subsequent text runs */
		size_t del_len = de - ds;
		run->len_bytes -= del_len;
		for ( i=nrun+1; i<para->n_runs; i++ ) {
			if ( para->runs[i].scblock == run->scblock ) {
				para->runs[i].scblock_offs_bytes -= del_len;
			}
			para->runs[i].para_offs_bytes -= del_len;
		}
		offs2 -= del_len;

	}
}


static char *run_text(struct text_run *run)
{
	return strndup(sc_block_contents(run->scblock)+run->scblock_offs_bytes,
	               run->len_bytes);
}


void show_para(Paragraph *p)
{
	int i;
	printf("Paragraph %p\n", p);
	printf("%i runs:\n", p->n_runs);
	for ( i=0; i<p->n_runs; i++ ) {
		char *tmp = run_text(&p->runs[i]);
		printf("  Run %2i: para offs %lli, SCBlock %p offs %lli, len "
		       "%lli %s '%s'\n",
		       i, (long long int)p->runs[i].para_offs_bytes,
		       p->runs[i].scblock,
		       (long long int)p->runs[i].scblock_offs_bytes,
		       (long long int)p->runs[i].len_bytes,
		       pango_font_description_to_string(p->runs[i].fontdesc),
		       tmp);
		free(tmp);
	}
}


void merge_paragraphs(struct frame *fr, int para)
{
	Paragraph *p1, *p2;
	struct text_run *runs_new;
	int i, j;
	size_t offs;
	SCBlock *scblock;
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

	/* All the runs from p2 get added to p1 */
	runs_new = realloc(p1->runs,
	                   (p1->n_runs+p2->n_runs)*sizeof(struct text_run));
	if ( runs_new == NULL ) {
		fprintf(stderr, "Failed to allocate merged runs.\n");
		return;
	}
	p1->runs = runs_new;

	/* Locate the newline which we have just deleted. */
	scblock = p1->runs[p1->n_runs-1].scblock;
	n = sc_block_next(scblock);
	offs = p1->runs[p1->n_runs-1].scblock_offs_bytes;
	offs += p1->runs[p1->n_runs-1].len_bytes;

	if ( sc_block_contents(scblock)[offs] == '\n' ) {

		scblock_delete_text(scblock, offs, offs+1);

		/* Update the SC offset of any run from this SCBlock */
		for ( i=para+1; i<fr->n_paras; i++ ) {
			int done = 0;
			if ( fr->paras[i]->type != PARA_TYPE_TEXT ) break;
			for ( j=0; j<fr->paras[i]->n_runs; j++ ) {
				struct text_run *run = &fr->paras[i]->runs[j];
				if ( run->scblock == scblock ) {
					run->scblock_offs_bytes -= 1;
				} else {
					done = 1;
					break;
				}
			}
			if ( done ) break;
		}

	} else if ( (n!=NULL) && (sc_block_contents(n)[0] == '\n') ) {

		/* It's in the following SCBlock instead */

		const char *c = sc_block_contents(n);
		if ( strlen(c) == 1 ) {
			SCBlock *ss = scblock;
			sc_block_delete(&scblock, n);
			assert(ss == scblock);
		} else {
			scblock_delete_text(n, 0, 1);
		}

	} else {
		printf("Couldn't find newline!\n");
		printf("Have '%s'\n", sc_block_contents(scblock)+offs);
	}

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

	for ( i=para+1; i<fr->n_paras-1; i++ ) {
		fr->paras[i] = fr->paras[i+1];
	}
	fr->n_paras--;
}


static char *s_strdup(const char *a)
{
	if ( a == NULL ) return NULL;
	return strdup(a);
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

	/* First run of the new paragraph contains the leftover text */
	rr = &para->runs[run];
	pnew->runs[0].scblock = rr->scblock;
	pnew->runs[0].macro_real_block = rr->macro_real_block;
	run_offs = pos - rr->para_offs_bytes;
	pnew->runs[0].scblock_offs_bytes = rr->scblock_offs_bytes + run_offs;
	pnew->runs[0].para_offs_bytes = 0;
	pnew->runs[0].len_bytes = rr->len_bytes - run_offs;
	pnew->runs[0].fontdesc = pango_font_description_copy(rr->fontdesc);
	pnew->runs[0].col[0] = rr->col[0];
	pnew->runs[0].col[1] = rr->col[1];
	pnew->runs[0].col[2] = rr->col[2];
	pnew->runs[0].col[3] = rr->col[3];
	pnew->n_runs = 1;

	/* All later runs just get moved to the new paragraph */
	offs = pnew->runs[0].len_bytes;
	for ( i=run+1; i<para->n_runs; i++ ) {
		pnew->runs[pnew->n_runs] = para->runs[i];
		pnew->runs[pnew->n_runs].para_offs_bytes = offs;
		pnew->n_runs++;
		offs += rr->len_bytes;
	}

	/* Truncate the first paragraph at the appropriate position */
	rr->len_bytes = run_offs;
	para->n_runs = run+1;

	/* If the first and second paragraphs have the same SCBlock, split it */
	if ( rr->scblock == pnew->runs[0].scblock ) {
		size_t sc_offs;
		sc_offs = rr->scblock_offs_bytes + run_offs;
		pnew->runs[0].scblock = sc_block_split(rr->scblock, sc_offs);
		pnew->runs[0].scblock_offs_bytes = 0;
	}

	/* Add a newline after the end of the first paragraph's SC */
	sc_block_append(rr->scblock, s_strdup(sc_block_name(rr->scblock)),
	                             s_strdup(sc_block_options(rr->scblock)),
	                             strdup("\n"), NULL);

	pnew->open = para->open;
	para->open = 0;

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

	return para->macro_real_scblock;
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

	*ppos = run->scblock_offs_bytes + pos - run->para_offs_bytes;
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
