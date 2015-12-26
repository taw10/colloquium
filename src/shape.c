/*
 * shape.c
 *
 * Copyright © 2014-2015 Thomas White <taw@bitwiz.org.uk>
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

#include "wrap.h"
#include "sc_interp.h"
#include "shape.h"


static void shape_segment(struct wrap_box *box, struct text_seg *seg)
{
	PangoRectangle rect;
	const char *tp;
	const char *ep;

	tp = g_utf8_offset_to_pointer(sc_block_contents(box->scblock),
	                              box->offs_char);
	ep = g_utf8_offset_to_pointer(sc_block_contents(box->scblock),
	                              box->offs_char+box->len_chars);

	if ( seg->glyphs != NULL ) {
		pango_glyph_string_free(seg->glyphs);
	}
	seg->glyphs = pango_glyph_string_new();
	pango_shape(tp, ep-tp, &seg->analysis, seg->glyphs);

	pango_glyph_string_extents(seg->glyphs, box->font, NULL, &rect);

	seg->width = rect.width;
	seg->height = rect.height;
	seg->ascent = PANGO_ASCENT(rect);
}


static void calc_box_geometry(struct wrap_box *box)
{
	int i;

	if ( box->type != WRAP_BOX_PANGO ) return;

	box->width = 0;
	box->height = 0;
	box->ascent = 0;

	for ( i=0; i<box->n_segs; i++ ) {
		struct text_seg *seg = &box->segs[i];
		box->width += seg->width;
		if ( seg->height > box->height ) box->height = seg->height;
		if ( seg->ascent > box->ascent ) box->ascent = seg->ascent;
	}
}


void shape_box(struct wrap_box *box)
{
	int i;

	for ( i=0; i<box->n_segs; i++ ) {
		shape_segment(box, &box->segs[i]);
	}

	calc_box_geometry(box);
}


static void add_nothing_box(struct wrap_line *line, SCBlock *scblock,
                            int editable, enum wrap_box_space sp,
                            SCInterpreter *scin, size_t offs)
{
	struct wrap_box *box;

	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return;
	}

	box = &line->boxes[line->n_boxes];
	box->type = WRAP_BOX_NOTHING;
	box->scblock = scblock;
	box->offs_char = offs;
	box->space = sp;
	box->width = 0;
	box->len_chars = 0;
	box->ascent = sc_interp_get_ascent(scin);
	box->height = sc_interp_get_height(scin);
	box->filename = NULL;
	box->editable = editable;
	line->n_boxes++;
}


static UNUSED char *swizzle(const char *inp, size_t len)
{
	int i;
	char *out = malloc(len+1);
	strncpy(out, inp, len);
	out[len] = '\0';
	for ( i=0; i<len; i++ ) {
		if ( out[i] == '\n' ) out[i] = '`';
	}
	return out;
}


static UNUSED void debug_log_attrs(size_t len_chars, const char *text,
                                   PangoLogAttr *log_attrs)
{
	int i;
	const gchar *p = text;

	if ( !g_utf8_validate(text, -1, NULL) ) {
		fprintf(stderr, "Invalid UTF8!\n");
		return;
	}

	for ( i=0; i<len_chars; i++ ) {
		gunichar c = g_utf8_get_char(p);
		p = g_utf8_next_char(p);
		if ( c == '\n' ) {
			printf("`");
		} else {
			printf("%lc", c);
		}
	}
	printf("\n");
	for ( i=0; i<=len_chars; i++ ) {
		if ( log_attrs[i].is_line_break ) {
			if ( log_attrs[i].is_mandatory_break ) {
				printf("n");
			} else {
				printf("b");
			}
		} else {
			printf(".");
		}
	}
	printf("\n");
}


static void add_seg(gpointer vi, gpointer vb)
{
	PangoItem *item = vi;
	struct wrap_box *box = vb;

	box->segs[box->n_segs].analysis = item->analysis;
	box->segs[box->n_segs].glyphs = NULL;
	box->segs[box->n_segs].offs_char = item->offset;
	box->segs[box->n_segs].len_chars = item->num_chars;
	shape_segment(box, &box->segs[box->n_segs++]);
}


/* Add "text", followed by a space of type "space", to "line" */
static int add_text_box(struct wrap_line *line,
                        enum wrap_box_space space, PangoContext *pc,
                        SCInterpreter *scin, SCBlock *bl, size_t offs,
                        size_t len, int editable)
{
	GList *pango_items;
	struct wrap_box *box;
	PangoAttrList *attrs;
	PangoAttribute *attr;
	const char *tp;
	double *col;
	int nseg;

	while ( len==0 ) {
		add_nothing_box(line, bl, editable, space, scin, offs);
		return 0;
	}

	/* Create the box */
	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return 1;
	}
	box = &line->boxes[line->n_boxes];

	box->type = WRAP_BOX_PANGO;
	box->space = space;
	box->font = sc_interp_get_font(scin);
	box->width = 0;
	box->editable = editable;
	box->ascent = sc_interp_get_ascent(scin);
	box->height = sc_interp_get_height(scin);
	box->cf = NULL;

	/* Link to the actual text */
	box->scblock = bl;
	tp = sc_block_contents(bl);
	box->offs_char = g_utf8_pointer_to_offset(tp, tp+offs);
	box->len_chars = g_utf8_strlen(tp+offs, len);

	col = sc_interp_get_fgcol(scin);
	box->col[0] = col[0];  /* Red */
	box->col[1] = col[1];  /* Green */
	box->col[2] = col[2];  /* Blue */
	box->col[3] = col[3];  /* Alpha */

	attrs = pango_attr_list_new();
	attr = pango_attr_font_desc_new(sc_interp_get_fontdesc(scin));
	pango_attr_list_insert_before(attrs, attr);
	pango_items = pango_itemize(pc, sc_block_contents(bl)+offs,
	                            0, len, attrs, NULL);
	nseg = g_list_length(pango_items);
	box->segs = malloc(nseg * sizeof(struct text_seg));
	if ( box->segs == NULL ) return 1;

	box->n_segs = 0;
	g_list_foreach(pango_items, add_seg, box);
	g_list_free(pango_items);
	pango_attr_list_unref(attrs);

	calc_box_geometry(box);

	line->n_boxes++;

	return 0;
}


void add_callback_box(struct wrap_line *line, double w, double h,
                      SCCallbackDrawFunc draw_func,
                      SCCallbackClickFunc click_func, void *bvp, void *vp)
{
	struct wrap_box *box;

	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return;
	}
	box = &line->boxes[line->n_boxes];

	box->type = WRAP_BOX_CALLBACK;
	box->scblock = NULL;
	box->offs_char = 0;
	box->space = WRAP_SPACE_NONE;
	box->width = pango_units_from_double(w);
	box->ascent = pango_units_from_double(h);
	box->height = pango_units_from_double(h);
	box->draw_func = draw_func;
	box->click_func = click_func;
	box->bvp = bvp;
	box->vp = vp;
	box->editable = 0;
	line->n_boxes++;
}


void add_image_box(struct wrap_line *line, const char *filename,
                   int w, int h, int editable)
{
	struct wrap_box *box;

	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return;
	}

	box = &line->boxes[line->n_boxes];
	box->type = WRAP_BOX_IMAGE;
	box->scblock = NULL;
	box->offs_char = 0;
	box->space = WRAP_SPACE_NONE;
	box->width = pango_units_from_double(w);
	box->ascent = pango_units_from_double(h);
	box->height = pango_units_from_double(h);
	box->filename = strdup(filename);
	box->editable = editable;
	line->n_boxes++;
}


int split_words(struct wrap_line *boxes, PangoContext *pc, SCBlock *bl,
                PangoLanguage *lang, int editable, SCInterpreter *scin)
{
	PangoLogAttr *log_attrs;
	glong len_chars, i;
	size_t len_bytes, start;
	int chars_done;
	const char *text = sc_block_contents(bl);

	/* Empty block? */
	if ( text == NULL ) return 1;

	len_chars = g_utf8_strlen(text, -1);
	if ( len_chars == 0 ) {
		add_text_box(boxes, WRAP_SPACE_NONE, pc, scin, bl,
		             0, 0, editable);
		return 1;
	}

	len_bytes = strlen(text);

	log_attrs = malloc((len_chars+1)*sizeof(PangoLogAttr));
	if ( log_attrs == NULL ) return 1;

	pango_get_log_attrs(text, len_bytes, -1, lang, log_attrs, len_chars+1);

	//debug_log_attrs(len_chars, text, log_attrs);

	start = 0;
	chars_done = 0;
	for ( i=0; i<len_chars; i++ ) {

		if ( log_attrs[i].is_line_break ) {

			enum wrap_box_space type;
			size_t len;
			char *ptr;
			size_t offs;

			ptr = g_utf8_offset_to_pointer(text, i);
			offs = ptr - text;

			/* Stuff up to (but not including) sc[i] forms a
			 * wrap box */
			len = offs - start;
			/* FIXME: Ugh */
			if ( log_attrs[i].is_mandatory_break ) {
				type = WRAP_SPACE_EOP;
				if ( (i>0) && (g_utf8_prev_char(ptr)[0]=='\n') ) len--;
			} else if ( (i>0)
			         && log_attrs[i-1].is_expandable_space ) {
				type = WRAP_SPACE_INTERWORD;
				len--;
			} else {
				type = WRAP_SPACE_NONE;
			}

			if ( add_text_box(boxes, type, pc, scin, bl,
			                  start, len, editable) ) {
				fprintf(stderr, "Failed to add wrap box.\n");
			}
			start = offs;
			chars_done = i;

		}

	}

	/* Add the stuff left over at the end */
	if ( i > chars_done ) {

		size_t l = strlen(text+start);

		if ( (text[start+l-1] == '\n')  ) {

			/* There is a newline at the end of the SC */
			add_text_box(boxes, WRAP_SPACE_EOP, pc, scin, bl,
			             start, l-1, editable);

		} else {

			add_text_box(boxes, WRAP_SPACE_NONE, pc, scin, bl,
			             start, l, editable);

		}

	}

	free(log_attrs);
	return 0;
}
