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
#include "utils.h"

struct text_run
{
	SCBlock              *scblock;
	char                 *real_text;  /* Usually NULL */
	PangoFontDescription *fontdesc;
	double                col[4];
};

struct _paragraph
{
	enum para_type   type;
	double           height;
	float            space[4];
	SCBlock         *newline_at_end;
	int              empty;

	/* For PARA_TYPE_TEXT */
	int              n_runs;
	struct text_run *runs;
	PangoLayout     *layout;
	PangoAlignment   alignment;

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


/* Returns the height of the paragraph including all spacing, padding etc */
double paragraph_height(Paragraph *para)
{
	return para->height + para->space[2] + para->space[3];
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
		free(para->runs[i].real_text);  /* free(NULL) is OK */
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


void show_frame_hierarchy(struct frame *fr, const char *t)
{
	int i;
	char tn[1024];

	strcpy(tn, t);
	strcat(tn, "      ");

	printf("%s%p (%.2f x %.2f)\n", t, fr, fr->w, fr->h);

	for ( i=0; i<fr->num_children; i++ ) {
		show_frame_hierarchy(fr->children[i], tn);
	}

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


static const char *text_for_run(const struct text_run *run)
{
	if ( run == NULL ) {
		fprintf(stderr, "NULL run passed to text_for_run\n");
		return 0;
	}

	if ( run->scblock == NULL ) {
		fprintf(stderr, "NULL scblock in text_for_run\n");
		return 0;
	}

	if ( run->real_text != NULL ) {
		return run->real_text;
	}

	return sc_block_contents(run->scblock);
}


static size_t run_text_len(const struct text_run *run)
{
	if ( run == NULL ) {
		fprintf(stderr, "NULL run passed to run_text_len\n");
		return 0;
	}

	if ( run->scblock == NULL ) {
		fprintf(stderr, "NULL scblock in run_text_len\n");
		return 0;
	}

	if ( run->real_text != NULL ) {
		return strlen(run->real_text);
	}

	if ( sc_block_contents(run->scblock) == NULL ) {
		fprintf(stderr, "NULL scblock contents in run_text_len\n");
		return 0;
	}

	return strlen(sc_block_contents(run->scblock));
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
		total_len += run_text_len(&para->runs[i]);
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
		size_t run_len;
		guint16 r, g, b;

		run_text = text_for_run(&para->runs[i]);
		run_len = strlen(run_text);

		attr = pango_attr_font_desc_new(para->runs[i].fontdesc);
		attr->start_index = pos;
		attr->end_index = pos + run_len;
		pango_attr_list_insert(attrs, attr);

		r = para->runs[i].col[0] * 65535;
		g = para->runs[i].col[1] * 65535;
		b = para->runs[i].col[2] * 65535;
		attr = pango_attr_foreground_new(r, g, b);
		attr->start_index = pos;
		attr->end_index = pos + run_len;
		pango_attr_list_insert(attrs, attr);

		pos += run_len;
		strncat(text, run_text, run_len);

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
	pango_layout_set_alignment(para->layout, para->alignment);
	pango_layout_set_attributes(para->layout, attrs);
	free(text);
	pango_attr_list_unref(attrs);

	pango_layout_get_extents(para->layout, NULL, &rect);
	para->height = pango_units_to_double(rect.height);
}

static SCBlock *get_newline_at_end(Paragraph *para)
{
	return para->newline_at_end;
}


SCBlock *para_debug_get_newline_at_end(Paragraph *para)
{
	return get_newline_at_end(para);
}


void set_newline_at_end(Paragraph *para, SCBlock *bl)
{
	para->newline_at_end = bl;
}


void add_run(Paragraph *para, SCBlock *scblock,
             PangoFontDescription *fdesc, double col[4], const char *real_text)
{
	struct text_run *runs_new;

	runs_new = realloc(para->runs,
	                   (para->n_runs+1)*sizeof(struct text_run));
	if ( runs_new == NULL ) {
		fprintf(stderr, "Failed to add run.\n");
		return;
	}

	para->runs = runs_new;
	para->runs[para->n_runs].scblock = scblock;
	if ( real_text != NULL ) {
		para->runs[para->n_runs].real_text = strdup(real_text);
	} else {
		para->runs[para->n_runs].real_text = NULL;
	}
	para->runs[para->n_runs].fontdesc = pango_font_description_copy(fdesc);
	para->runs[para->n_runs].col[0] = col[0];
	para->runs[para->n_runs].col[1] = col[1];
	para->runs[para->n_runs].col[2] = col[2];
	para->runs[para->n_runs].col[3] = col[3];
	para->empty = 0;
	para->n_runs++;
}


Paragraph *create_paragraph(struct frame *fr, SCBlock *bl)
{
	Paragraph **paras_new;
	Paragraph *pnew;

	paras_new = realloc(fr->paras, (fr->n_paras+1)*sizeof(Paragraph *));
	if ( paras_new == NULL ) return NULL;

	pnew = calloc(1, sizeof(struct _paragraph));
	if ( pnew == NULL ) return NULL;

	fr->paras = paras_new;
	fr->paras[fr->n_paras++] = pnew;

	/* For now, assume the paragraph is going to be for text.
	 * However, this can easily be changed */
	pnew->type = PARA_TYPE_TEXT;
	pnew->scblock = bl;
	pnew->n_runs = 0;
	pnew->runs = NULL;
	pnew->layout = NULL;
	pnew->height = 0.0;
	pnew->alignment = PANGO_ALIGN_LEFT;
	pnew->empty = 1;

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

	fr->paras = paras_new;
	fr->n_paras++;

	for ( i=fr->n_paras-1; i>pos; i-- ) {
		fr->paras[i] = fr->paras[i-1];
	}
	fr->paras[pos+1] = pnew;

	return pnew;
}


Paragraph *add_callback_para(struct frame *fr, SCBlock *bl, double w, double h,
                             SCCallbackDrawFunc draw_func,
                             SCCallbackClickFunc click_func, void *bvp,
                             void *vp)
{
	Paragraph *pnew;

	if ( (fr->n_paras > 0) && (fr->paras[fr->n_paras-1]->empty) ) {
		pnew = fr->paras[fr->n_paras-1];
	} else {
		pnew = create_paragraph(fr, bl);
		if ( pnew == NULL ) {
			fprintf(stderr, "Failed to add callback paragraph\n");
			return NULL;
		}
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
	pnew->empty = 0;

	return pnew;
}


void add_image_para(struct frame *fr, SCBlock *scblock,
                    const char *filename,
                    ImageStore *is, double w, double h, int editable)
{
	Paragraph *pnew;
	int wi, hi;

	if ( is == NULL ) {
		fprintf(stderr, "Adding image without ImageStore!\n");
		return;
	}

	if ( (fr->n_paras > 0) && (fr->paras[fr->n_paras-1]->empty) ) {
		pnew = fr->paras[fr->n_paras-1];
	} else {
		pnew = create_paragraph(fr, scblock);
		if ( pnew == NULL ) {
			fprintf(stderr, "Failed to add image paragraph\n");
			return;
		}
	}

	if ( imagestore_get_size(is, filename, &wi, &hi) ) {
		fprintf(stderr, _("Couldn't determine size of image '%s'\n"),
		        filename);
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
	pnew->space[0] = 0.0;
	pnew->space[1] = 0.0;
	pnew->space[2] = 0.0;
	pnew->space[3] = 0.0;
	pnew->empty = 0;
}


double total_height(struct frame *fr)
{
	int i;
	double t = 0.0;
	for ( i=0; i<fr->n_paras; i++ ) {
		t += paragraph_height(fr->paras[i]);
	}
	return t;
}


Paragraph *last_para(struct frame *fr)
{
	if ( fr == NULL ) return NULL;
	if ( fr->paras == NULL ) return NULL;
	return fr->paras[fr->n_paras-1];
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


static size_t end_offset_of_para(struct frame *fr, int pn)
{
	int i;
	size_t total = 0;
	for ( i=0; i<fr->paras[pn]->n_runs; i++ ) {
		total += run_text_len(&fr->paras[pn]->runs[i]);
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


static PangoFontDescription *last_font_desc_and_col(struct frame *fr, int pn, double *col)
{
	int i;

	for ( i=pn-1; i>=0; i-- ) {
		if ( fr->paras[i]->type != PARA_TYPE_TEXT ) continue;
		if ( fr->paras[i]->n_runs == 0 ) continue;
		col[0] = fr->paras[i]->runs[fr->paras[i]->n_runs-1].col[0];
		col[1] = fr->paras[i]->runs[fr->paras[i]->n_runs-1].col[1];
		col[2] = fr->paras[i]->runs[fr->paras[i]->n_runs-1].col[2];
		col[3] = fr->paras[i]->runs[fr->paras[i]->n_runs-1].col[3];
		return fr->paras[i]->runs[fr->paras[i]->n_runs-1].fontdesc;
	}

	/* No previous text at all, so use the default text style
	 * (which is valid for new text in the frame, so this is OK) */
	col[0] = fr->col[0];
	col[1] = fr->col[1];
	col[2] = fr->col[2];
	col[3] = fr->col[3];
	return fr->fontdesc;
}


void ensure_run(struct frame *fr, struct edit_pos cpos)
{
	SCBlock *bl;
	Paragraph *para;
	PangoFontDescription *fontdesc;
	double col[4];

	para = fr->paras[cpos.para];

	if ( para->n_runs > 0 ) return;

	if ( para->type != PARA_TYPE_TEXT ) return;

	if ( para->scblock != NULL ) {

		bl = sc_block_prepend(para->scblock, fr->scblocks);
		if ( bl == NULL ) {
			fprintf(stderr, "Couldn't prepend block\n");
			return;
		}
		sc_block_set_contents(bl, strdup(""));

	} else {

		/* If the paragraph's SCBlock is NULL, it means this paragraph
		 * is right at the end of the document.  The last thing in the
		 * document is something like \newpara. */
		bl = sc_block_append_inside(fr->scblocks, NULL, NULL, strdup(""));

	}

	para->scblock = bl;

	fontdesc = last_font_desc_and_col(fr, cpos.para, col);
	add_run(para, bl, fontdesc, col, NULL);
	wrap_paragraph(para, NULL, fr->w - fr->pad_l - fr->pad_r, 0, 0);
}


int find_cursor(struct frame *fr, double x, double y, struct edit_pos *pos)
{
	double pad;
	int i;

	if ( fr == NULL ) {
		fprintf(stderr, "Cursor frame is NULL.\n");
		return 1;
	}

	pad = fr->pad_t;

	for ( i=0; i<fr->n_paras; i++ ) {
		double npos = pad + paragraph_height(fr->paras[i]);
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
		py += paragraph_height(fr->paras[i]);
	}

	*cx = fr->pad_l;
	*cy = fr->pad_t + py;
	*cw = fr->w - fr->pad_l - fr->pad_r;
	*ch = paragraph_height(para);
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
		py += paragraph_height(fr->paras[i]);
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


//void cursor_moveh(struct frame *fr, int *cpara, size_t *cpos, int *ctrail,
void cursor_moveh(struct frame *fr, struct edit_pos *cp, signed int dir)
{
	Paragraph *para = fr->paras[cp->para];
	int np = cp->pos;

	pango_layout_move_cursor_visually(para->layout, 1, cp->pos, cp->trail,
	                                  dir, &np, &cp->trail);
	if ( np == -1 ) {
		if ( cp->para > 0 ) {
			size_t end_offs;
			cp->para--;
			end_offs = end_offset_of_para(fr, cp->para);
			if ( end_offs > 0 ) {
				cp->pos = end_offs - 1;
				cp->trail = 1;
			} else {
				/* Jumping into an empty paragraph */
				cp->pos = 0;
				cp->trail = 0;
			}
			return;
		} else {
			/* Can't move any further */
			return;
		}
	}

	if ( np == G_MAXINT ) {
		if ( cp->para < fr->n_paras-1 ) {
			cp->para++;
			cp->pos = 0;
			cp->trail = 0;
			return;
		} else {
			/* Can't move any further */
			return;
		}
	}

	cp->pos = np;
}


void check_callback_click(struct frame *fr, int para)
{
	Paragraph *p = fr->paras[para];
	if ( p->type == PARA_TYPE_CALLBACK ) {
		p->click_func(0.0, 0.0, p->bvp, p->vp);
	}
}


static int get_paragraph_offset(Paragraph *para, int nrun)
{
	int i;
	size_t t = 0;

	for ( i=0; i<nrun; i++ ) {
		struct text_run *run = &para->runs[i];
		t += run_text_len(run);
	}
	return t;
}


static int which_run(Paragraph *para, size_t offs)
{
	int i;
	size_t t = 0;

	for ( i=0; i<para->n_runs; i++ ) {
		struct text_run *run = &para->runs[i];
		t += run_text_len(run);
		if ( t > offs ) return i;
	}

	/* Maybe offs points exactly to the end of the last run? */
	if ( t == offs ) return para->n_runs-1;

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
	size_t para_offset_of_run;

	nrun = which_run(para, offs);

	if ( nrun == para->n_runs ) {
		fprintf(stderr, "pos_trail_to_offset: Offset too high\n");
		return 0;
	}

	if ( para->n_runs == 0 ) {
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

	/* Get the text for the run */
	run_text = text_for_run(run);
	if ( run_text == NULL ) {
		fprintf(stderr, "pos_trail_to_offset: No contents "
		        "(%p name=%s, options=%s)\n",
		        run->scblock, sc_block_name(run->scblock),
		        sc_block_options(run->scblock));
		return 0;
	}


	/* Turn  the paragraph offset into a run offset */
	para_offset_of_run = get_paragraph_offset(para, nrun);
	run_offs = offs - para_offset_of_run;

	char_offs = g_utf8_pointer_to_offset(run_text, run_text+run_offs);
	char_offs += trail;

	if ( char_offs > g_utf8_strlen(run_text, -1) ) {
		printf("Offset outside string! '%s'\n"
		       "char_offs %li offs %li len %li\n",
		       run_text, (long int)char_offs, (long int)offs,
		       (long int)g_utf8_strlen(run_text, -1));
	}

	ptr = g_utf8_offset_to_pointer(run_text, char_offs);
	return ptr - run_text + para_offset_of_run;
}


int position_editable(struct frame *fr, struct edit_pos cp)
{
	Paragraph *para;
	int run;
	size_t paraoffs;

	if ( fr == NULL ) {
		fprintf(stderr, "Frame is NULL.\n");
		return 0;
	}

	if ( cp.para >= fr->n_paras ) {
		fprintf(stderr, "Paragraph number is too high!\n");
		return 0;
	}

	para = fr->paras[cp.para];

	if ( para->type != PARA_TYPE_TEXT ) {
		fprintf(stderr, "Paragraph is not text.\n");
		return 0;
	}

	paraoffs = pos_trail_to_offset(para, cp.pos, cp.trail);
	run = which_run(para, paraoffs);
	if ( run == para->n_runs ) {
		fprintf(stderr, "Couldn't find run!\n");
		return 0;
	}

	if ( para->runs[run].real_text != NULL ) return 0;

	return 1;
}


void insert_text_in_paragraph(Paragraph *para, size_t offs, const char *t)
{
	int nrun;

	/* Find which run we are in */
	nrun = which_run(para, offs);
	if ( nrun == para->n_runs ) {
		fprintf(stderr, "Couldn't find run to insert into.\n");
		return;
	}

	if ( para->n_runs == 0 ) {
		printf("No runs in paragraph?\n");
	} else {
		struct text_run *run;
		size_t run_offs;
		run = &para->runs[nrun];
		run_offs = offs - get_paragraph_offset(para, nrun);
		sc_insert_text(run->scblock, run_offs, t);
	}
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

	/* Offset of this position into the paragraph */
	paraoffs = pos_trail_to_offset(para, p.pos, p.trail);

	run = which_run(para, paraoffs);
	assert(run < para->n_runs);

	/* Offset of this position into the run
	 * (and therefore into the SCBlock) */
	return paraoffs - get_paragraph_offset(para, run);
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


static void delete_run(Paragraph *para, int nrun)
{
	printf("deleting run %i of %i from para %p\n", nrun, para->n_runs, para);
	memmove(&para->runs[nrun], &para->runs[nrun+1],
	        (para->n_runs-nrun-1)*sizeof(struct text_run));
	para->n_runs--;
}


static Paragraph *scan_runs_for_scblock(struct frame *fr, int pn1, int pn2,
                                        SCBlock *bl, int *run)
{
	int i;

	for ( i=pn1; i<=pn2; i++ ) {

		int j;

		/* Non-text paragraph - just one thing to check */
		if ( fr->paras[i]->scblock == bl ) {
			*run = 0;
			return fr->paras[i];
		}

		/* Check all runs */
		for ( j=0; j<fr->paras[i]->n_runs; j++ ) {
			if ( fr->paras[i]->runs[j].scblock == bl ) {
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


static int find_para(struct frame *fr, Paragraph *para)
{
	int i;

	for ( i=0; i<fr->n_paras; i++ ) {
		if ( fr->paras[i] == para ) return i;
	}

	return fr->n_paras;
}


static void delete_paragraph(struct frame *fr, Paragraph *para, int *pnp)
{
	int pn = find_para(fr, para);
	if ( pn == fr->n_paras ) {
		fprintf(stderr, "Couldn't find paragraph to delete (%p)\n", para);
		return;
	}

	printf("deleting paragraph %i (%p)\n", pn, para);

	memmove(&fr->paras[pn], &fr->paras[pn+1],
	        (fr->n_paras-pn-1)*sizeof(Paragraph *));
	fr->n_paras--;

	if ( (pnp != NULL) && (*pnp > pn) ) {
		(*pnp)--;
	}
}


static void delete_run_for_scblock(struct frame *fr,
                                   Paragraph *p1, Paragraph *p2, SCBlock *bl,
                                   int *pnp)
{
	int pn1, pn2;
	int err = 0;
	Paragraph *para;
	int run;

	pn1 = paragraph_number(fr, p1, &err);
	pn2 = paragraph_number(fr, p2, &err);
	if ( err ) return;

	para = find_run_for_scblock(fr, pn1, pn2, bl, &run);
	if ( para == NULL ) {
		fprintf(stderr, "Couldn't find block %p between paragraphs %p and %p\n",
		        bl, p1, p2);
		return;
	}

	if ( (run==0) && (para->scblock == bl ) ) {
		delete_paragraph(fr, para, pnp);
	} else {
		delete_run(para, run);
	}
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

	spos = p1->n_runs;

	/* The end of the united paragraph should now be the end of the
	 * second one */
	set_newline_at_end(p1, get_newline_at_end(p2));

	for ( i=0; i<p2->n_runs; i++ ) {
		p1->runs[p1->n_runs] = p2->runs[i];
		p1->n_runs++;
	}
	free(p2->runs);
	free(p2);

	return spos;
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
	enum para_type type1, type2;
	size_t p2offs;
	SCBlock *scblock;
	int wrap_end;

	sort_positions(&p1, &p2);

	/* To make sure there are no nasty surprises ahead, run through the
	 * paragraphs we're about to touch, and make sure they all have at least
	 * an empty dummy run */
	for ( i=p1.para; i<=p2.para; i++ ) {
		struct edit_pos ep;
		ep.para = i;
		ep.pos = 0;
		ep.trail = 0;
		ensure_run(fr, ep);
	}

	if ( !position_editable(fr, p1) || !position_editable(fr, p2) ) {
		fprintf(stderr, "Delete outside editable region\n");
		return;
	}

	/* Find SC positions for start and end */
	p1scblock = pos_to_scblock(fr, p1, &type1);
	p2scblock = pos_to_scblock(fr, p2, &type2);
	p2offs = pos_to_offset(fr, p2);
	wrap_end = p2.para;

	printf("SCBlocks %p to %p\n", p1scblock, p2scblock);
	if ( p1scblock == NULL ) {
		fprintf(stderr, "Starting block NULL.  Not deleting.\n");
		return;
	}
	if ( p2scblock == NULL ) {
		fprintf(stderr, "Ending block NULL.  Not deleting.\n");
		return;
	}
	//show_sc_blocks(p1scblock);

	if ( (p1scblock == p2scblock) && (type1 == PARA_TYPE_TEXT) ) {

		size_t p1offs;
		printf("Simple case, one SCBlock\n");

		assert(type1 == type2);

		/* Remove the text and update the run length */
		p1offs = pos_to_offset(fr, p1);
		scblock_delete_text(p1scblock, p1offs, p2offs);

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
			printf("Partial delete\n");
			printf("contents '%s'\n", sc_block_contents(p1scblock));
			printf("from offs %li\n", (long int)p1offs);
			scblock_delete_text(p1scblock, p1offs, -1);
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
				p2.para--;
			}

			next = sc_block_next(scblock);
			delete_run_for_scblock(fr, fr->paras[p1.para],
			                       fr->paras[p2.para], scblock,
			                       &p2.para);
			sc_block_delete(&fr->scblocks, scblock);

			scblock = next;

		} while ( !block_is_under(p2scblock, scblock) );
	}

	/* Last SCBlock in range: delete whole thing or first half */
	printf("Last block %p (%s)\n", p2scblock, sc_block_name(p2scblock));
	if ( type2 == PARA_TYPE_TEXT ) {
		size_t len;
		printf("  offs %li\n", (long int)p2offs);
		if ( sc_block_contents(p2scblock) != NULL ) {
			len = strlen(sc_block_contents(p2scblock));
		} else {
			len = 0;
		}
		printf("  len %li\n", (long int)len);
		if ( (len > 0) && (p2offs == len) ) {
			printf("Deleting the whole text SCBlock\n");
			printf("deleting block %p\n", p2scblock);
			show_sc_block(p2scblock, "");
			sc_block_delete(&fr->scblocks, p2scblock);
			delete_run_for_scblock(fr, fr->paras[p1.para],
			                       fr->paras[p2.para], p2scblock,
			                       NULL);
		} else if ( p2offs > 0 ) {
			printf("Partial delete\n");
			printf("contents '%s'\n", sc_block_contents(p2scblock));
			printf("up to offs %li\n", (long int)p2offs);
			scblock_delete_text(p2scblock, 0, p2offs);
		} /* else do nothing */
	} else {
		printf("Deleting the whole non-text SCBlock\n");
		sc_block_delete(&fr->scblocks, p2scblock);
	}

	/* If any paragraphs have been deleted, this will wrap too many
	 * paragraphs, but it doesn't matter as long as we don't wrap
	 * past the end of the frame's contents. */
	for ( i=p1.para; i<=wrap_end; i++ ) {
		if ( i >= fr->n_paras ) break;
		printf("Wrapping para %i (%p)\n", i, fr->paras[i]);
		wrap_paragraph(fr->paras[i], NULL, wrapw, 0, 0);
	}
	printf("All done.\n");
}


void show_para(Paragraph *p)
{
	printf("Paragraph %p\n", p);

	if ( p->type == PARA_TYPE_TEXT ) {

		int i;

		printf("%i runs:\n", p->n_runs);
		for ( i=0; i<p->n_runs; i++ ) {
			printf("  Run %2i: SCBlock %p %s '%s'\n",
			       i, p->runs[i].scblock,
			       pango_font_description_to_string(p->runs[i].fontdesc),
			       sc_block_contents(p->runs[i].scblock));
		}

	} else if ( p->type == PARA_TYPE_IMAGE ) {
		printf("  Image: %s\n", p->filename);
	} else {
		printf("  Other paragraph type\n");
	}
}


static SCBlock *split_text_paragraph(struct frame *fr, int pn, size_t pos,
                                     PangoContext *pc)
{
	Paragraph *pnew;
	int i;
	SCBlock *nnp;
	size_t run_offs;
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

	/* Create the new (second) paragraph */
	pnew->type = PARA_TYPE_TEXT;
	pnew->n_runs = 0;
	pnew->runs = NULL;
	for ( i=0; i<4; i++ ) pnew->space[i] = para->space[i];

	rr = &para->runs[run];
	run_offs = pos - get_paragraph_offset(para, run);
	printf("split at run %i\n", run);

	/* Easy case: splitting at a run boundary */
	if ( run_offs == run_text_len(rr) ) {

		/* Even easier case: splitting at the end of the paragraph */
		if ( run == para->n_runs-1 ) {

			SCBlock *np;
			SCBlock *end;

			printf("Simple new para\n");

			if ( get_newline_at_end(para) == NULL ) {

				/* The current paragraph doesn't have
				 * a \newpara yet */

				np = sc_block_append(rr->scblock,
				                     strdup("newpara"), NULL,
				                     NULL, NULL);
				set_newline_at_end(para, np);

			} else {

				SCBlock *op;

				/* If the current paragraph did have \newpara,
				 * then the new one needs one too */
				np = sc_block_append(rr->scblock,
				                     strdup("newpara"),
				                     NULL, NULL, NULL);
				op = get_newline_at_end(para);
				set_newline_at_end(para, np);
				set_newline_at_end(pnew, op);


			}

			/* Add an empty run + SCBlock to type into */
			end = sc_block_append(np, NULL, NULL, strdup(""), NULL);

			pnew->n_runs = 0;
			add_run(pnew, end, rr->fontdesc, rr->col, NULL);
			pnew->scblock = end;

			wrap_paragraph(pnew, pc, fr->w - fr->pad_l - fr->pad_r, 0, 0);

			return end;

		}

	} else {

		/* Split the run (and SCBlock) into two */
		double col[4] = {0,0,0,0};
		struct text_run *rn;

		printf("Splitting run %i.  Before:\n", run);
		show_para(para);

		add_run(para, NULL, NULL, col, NULL);
		/* -2 here because add_run increased para->n_runs by 1 */
		memmove(&para->runs[run+2], &para->runs[run+1],
		        (para->n_runs - run - 2)*sizeof(struct text_run));

		rr = &para->runs[run]; /* Because add_run realloced the runs */
		rn = &para->runs[run+1];

		rn->scblock = sc_block_split(rr->scblock, run_offs);

		rn->fontdesc = pango_font_description_copy(rr->fontdesc);
		rn->col[0] = rr->col[0];
		rn->col[1] = rr->col[1];
		rn->col[2] = rr->col[2];
		rn->col[3] = rr->col[3];

		printf("After:\n");
		show_para(para);

	}

	/* All later runs just get moved to the new paragraph */
	for ( i=run+1; i<para->n_runs; i++ ) {
		double col[4] = {0,0,0,0};
		printf("Moving run %i to pos %i\n", i, pnew->n_runs);
		add_run(pnew, NULL, NULL, col, NULL);
		pnew->runs[pnew->n_runs-1] = para->runs[i];
	}
	pnew->scblock = pnew->runs[0].scblock;

	/* Truncate the first paragraph at the appropriate position */
	para->n_runs = run+1;

	printf("Final paragraphs:\n");
	printf("First:\n");
	show_para(para);
	printf("Second:\n");
	show_para(pnew);

	/* Add a \newpara after the end of the first paragraph's SC */
	nnp = sc_block_append(rr->scblock, strdup("newpara"), NULL, NULL, NULL);
	set_newline_at_end(pnew, get_newline_at_end(para));
	set_newline_at_end(para, nnp);

	wrap_paragraph(para, pc, fr->w - fr->pad_l - fr->pad_r, 0, 0);
	wrap_paragraph(pnew, pc, fr->w - fr->pad_l - fr->pad_r, 0, 0);

	return sc_block_next(nnp);
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

	*ppos = pos - get_paragraph_offset(para, nrun);
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


void set_para_alignment(Paragraph *para, PangoAlignment align)
{
	if ( para == NULL ) return;
	para->alignment = align;
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


int para_debug_run_info(Paragraph *para, int i, SCBlock **scblock)
{
	if ( para->type != PARA_TYPE_TEXT ) return 1;
	if ( i >= para->n_runs ) return 1;

	*scblock = para->runs[i].scblock;
	return 0;
}