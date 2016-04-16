/*
 * frame.h
 *
 * Copyright © 2013-2016 Thomas White <taw@bitwiz.org.uk>
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

typedef struct _paragraph Paragraph;

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

extern Paragraph *last_open_para(struct frame *fr);
extern void close_last_paragraph(struct frame *fr);

extern double paragraph_height(Paragraph *para);
extern void render_paragraph(cairo_t *cr, Paragraph *para, ImageStore *is,
                             enum is_size isz);

extern void add_run(Paragraph *para, SCBlock *scblock, size_t offs_bytes,
                    size_t len_bytes, PangoFontDescription *fdesc,
                    double col[4]);

extern void add_callback_para(struct frame *fr, double w, double h,
                              SCCallbackDrawFunc draw_func,
                              SCCallbackClickFunc click_func, void *bvp,
                              void *vp);

extern void add_image_para(struct frame *fr, const char *filename,
                           double w, double h, int editable);

extern void wrap_paragraph(Paragraph *para, PangoContext *pc, double w);

extern size_t end_offset_of_para(struct frame *fr, int pn);

extern int find_cursor(struct frame *fr, double x, double y,
                       int *ppara, size_t *ppos, int *ptrail);

extern int get_para_highlight(struct frame *fr, int cursor_para,
                              double *cx, double *cy, double *cw, double *ch);

extern int get_cursor_pos(struct frame *fr, int cursor_para, int cursor_pos,
                          double *cx, double *cy, double *ch);

extern void cursor_moveh(struct frame *fr, int *cpara, size_t *cpos, int *ctrail,
                         signed int dir);

extern void cursor_movev(struct frame *fr, int *cpara, size_t *cpos, int *ctrail,
                         signed int dir);

extern void check_callback_click(struct frame *fr, int para);

extern void insert_text_in_paragraph(Paragraph *para, size_t offs,
                                     const char *t);

extern void delete_text_in_paragraph(Paragraph *para,
                                     size_t offs1, size_t offs2);

extern void split_paragraph(struct frame *fr, int pn, int pos,
                            PangoContext *pc);

#endif	/* FRAME_H */
