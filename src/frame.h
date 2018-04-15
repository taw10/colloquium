/*
 * frame.h
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

#ifndef FRAME_H
#define FRAME_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pango/pango.h>
#include <cairo.h>

#include "sc_parse.h"
#include "sc_interp.h"
#include "imagestore.h"


typedef enum
{
	UNITS_SLIDE,
	UNITS_FRAC
} LengthUnits;


typedef enum
{
	GRAD_NONE,
	GRAD_HORIZ,
	GRAD_VERT
} GradientType;

enum para_type
{
	PARA_TYPE_TEXT,
	PARA_TYPE_IMAGE,
	PARA_TYPE_CALLBACK
};

typedef struct _paragraph Paragraph;


struct edit_pos
{
	int para;    /* Paragraph number */
	size_t pos;  /* Byte position within paragraph
	              * Yes, really.  See pango_layout_xy_to_index */
	int trail;
};


struct frame
{
	struct frame            **children;
	int                       num_children;
	int                       max_children;

	SCBlock                  *scblocks;

	Paragraph               **paras;
	int                       n_paras;
	int                       max_paras;

	/* The font which will be used by default for this frame */
	PangoFontDescription     *fontdesc;
	double                    col[4];

	/* The rectangle allocated to this frame, determined by the renderer */
	double                    x;
	double                    y;
	double                    w;
	double                    h;

	double                    pad_t;
	double                    pad_b;
	double                    pad_l;
	double                    pad_r;

	/* Background properties for this frame */
	double                    bgcol[4];
	double                    bgcol2[4];
	GradientType              grad;

	/* True if this frame should be deleted on the next mouse click */
	int                       empty;

	/* True if the aspect ratio of this frame should be maintained */
	int                       is_image;

	/* True if this frame can be resized and moved */
	int                       resizable;
};


extern struct frame *frame_new(void);
extern void frame_free(struct frame *fr);
extern struct frame *add_subframe(struct frame *fr);
extern void show_hierarchy(struct frame *fr, const char *t);
extern void delete_subframe(struct frame *top, struct frame *fr);
extern struct frame *find_frame_with_scblocks(struct frame *top,
                                              SCBlock *scblocks);

extern double total_height(struct frame *fr);

extern Paragraph *last_para(struct frame *fr);
extern void show_para(Paragraph *p);
extern void set_para_spacing(Paragraph *para, float space[4]);
extern void set_para_alignment(Paragraph *para, PangoAlignment align);

extern double paragraph_height(Paragraph *para);
extern void render_paragraph(cairo_t *cr, Paragraph *para, ImageStore *is);

extern SCBlock *get_newline_at_end(Paragraph *para);
extern void set_newline_at_end(Paragraph *para, SCBlock *bl);
extern void check_run(struct frame *fr, int pn);
extern void show_edit_pos(struct edit_pos a);

extern void add_run(Paragraph *para, SCBlock *scblock, SCBlock *rscblock,
                    PangoFontDescription *fdesc, double col[4]);

extern Paragraph *insert_paragraph(struct frame *fr, int pos);

extern Paragraph *add_callback_para(struct frame *fr, SCBlock *scblock,
                                    SCBlock *rscblock,
                                    double w, double h,
                                    SCCallbackDrawFunc draw_func,
                                    SCCallbackClickFunc click_func, void *bvp,
                                    void *vp);

extern void add_image_para(struct frame *fr, SCBlock *scblock,
                           SCBlock *rscblock, const char *filename,
                           ImageStore *is, double w, double h, int editable);

extern void wrap_paragraph(Paragraph *para, PangoContext *pc, double w,
                           size_t sel_start, size_t sel_end);

extern size_t end_offset_of_para(struct frame *fr, int pn);

extern int find_cursor(struct frame *fr, double x, double y,
                       struct edit_pos *pos);

extern void sort_positions(struct edit_pos *a, struct edit_pos *b);

extern void ensure_run(struct frame *fr, struct edit_pos cpos);

extern int positions_equal(struct edit_pos a, struct edit_pos b);

extern int get_para_highlight(struct frame *fr, int cursor_para,
                              double *cx, double *cy, double *cw, double *ch);

extern int position_editable(struct frame *fr, struct edit_pos cp);

extern int get_cursor_pos(struct frame *fr, int cursor_para, int cursor_pos,
                          double *cx, double *cy, double *ch);

extern void cursor_moveh(struct frame *fr, struct edit_pos *cp, signed int dir);

extern void cursor_movev(struct frame *fr, struct edit_pos *cp, signed int dir);

extern void check_callback_click(struct frame *fr, int para);

extern size_t pos_trail_to_offset(Paragraph *para, size_t offs, int trail);

extern void insert_text_in_paragraph(Paragraph *para, size_t offs,
                                     const char *t);

extern void delete_text_from_frame(struct frame *fr, struct edit_pos p1, struct edit_pos p2,
                                   double wrap_w);

extern size_t delete_text_in_paragraph(struct frame *fr, int npara, size_t offs0, ssize_t offs2);

extern SCBlock *split_paragraph(struct frame *fr, int pn, size_t pos,
                                PangoContext *pc);
extern SCBlock *block_at_cursor(struct frame *fr, int para, size_t pos);

extern int get_sc_pos(struct frame *fr, int pn, size_t pos,
                      SCBlock **bl, size_t *ppos);

extern void *get_para_bvp(Paragraph *para);

extern Paragraph *create_paragraph(struct frame *fr, SCBlock *bl, SCBlock *rbl);

extern enum para_type para_type(Paragraph *para);
extern SCBlock *para_scblock(Paragraph *para);
extern SCBlock *para_rscblock(Paragraph *para);

extern int para_debug_num_runs(Paragraph *para);
extern int para_debug_run_info(Paragraph *para, int i, SCBlock **scblock,
                               SCBlock **rscblock);

#endif	/* FRAME_H */
