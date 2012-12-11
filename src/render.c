/*
 * render.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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

#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <storycode.h>

#include "stylesheet.h"
#include "presentation.h"
#include "frame.h"
#include "render.h"


enum wrap_box_type
{
	WRAP_BOX_PANGO,
};


struct wrap_box
{
	enum wrap_box_type type;
	int width;  /* Pango units */

	/* For type == WRAP_BOX_PANGO */
	PangoGlyphItem *glyph_item;
	const char *text;
};


struct wrap_line
{
	int width;
	int height;  /* Pango units */

	int n_boxes;
	int max_boxes;
	struct wrap_box *boxes;
};


struct renderstuff
{
	cairo_t *cr;
	PangoContext *pc;
	PangoFontMap *fontmap;
	PangoAttrList *attrs;

	/* Text for the block currently being processed */
	const char *cur_text;
	int cur_len;

	int wrap_w;  /* Pango units */

	/* Lines of boxes, where each box can be a load of glyphs, an image,
	 * etc */
	int n_lines;
	int max_lines;
	struct wrap_line *lines;
};


static void alloc_lines(struct renderstuff *s)
{
	struct wrap_line *lines_new;

	lines_new = realloc(s->lines, s->max_lines * sizeof(struct wrap_line));
	if ( lines_new == NULL ) {
		fprintf(stderr, "Couldn't allocate memory for lines!\n");
		return;
	}

	s->lines = lines_new;
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


static void add_glyph_box_to_line(struct wrap_line *line, PangoGlyphItem *gi,
                                  char *text)
{
	PangoRectangle rect;

	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return;
	}

	pango_glyph_string_extents(gi->glyphs, gi->item->analysis.font,
	                           NULL, &rect);

	line->boxes[line->n_boxes].type = WRAP_BOX_PANGO;
	line->boxes[line->n_boxes].glyph_item = gi;
	line->boxes[line->n_boxes].text = text;
	line->boxes[line->n_boxes].width = rect.width;
	line->n_boxes++;

	line->width += rect.width;
	if ( line->height < rect.height ) line->height = rect.height;
}


static const char *add_chars_to_line(struct renderstuff *s,
                                     PangoGlyphItem *orig, int n,
                                     const char *cur_text_ptr)
{
	PangoGlyphItem *before;
	int split_len;
	char *split_ptr;
	char *before_text;

	split_ptr = g_utf8_offset_to_pointer(cur_text_ptr, n);
	before_text = strndup(cur_text_ptr, split_ptr-cur_text_ptr);
	if ( before_text == NULL ) {
		fprintf(stderr, "Failed to split text\n");
		/* But continue */
	}

	if ( n < orig->item->num_chars ) {

		split_len = split_ptr - cur_text_ptr;
		printf("splitting at %i/%i chars\n", n, orig->item->num_chars);
		printf("  -> %i/%i bytes\n", split_len, orig->item->length);

		before = pango_glyph_item_split(orig, cur_text_ptr, split_len);

		add_glyph_box_to_line(&s->lines[s->n_lines],
		                      before, before_text);

	} else {

		printf("adding final %i chars\n", n);
		add_glyph_box_to_line(&s->lines[s->n_lines], orig, before_text);

	}

	return split_ptr;
}


static void dispatch_line(struct renderstuff *s)
{
	if ( s->n_lines == s->max_lines ) {
		s->max_lines += 32;
		alloc_lines(s);
		if ( s->n_lines == s->max_lines ) return;
	}

	s->n_lines++;
	s->lines[s->n_lines].n_boxes = 0;
	s->lines[s->n_lines].max_boxes = 32;
	s->lines[s->n_lines].boxes = NULL;
	s->lines[s->n_lines].width = 0;
	alloc_boxes(&s->lines[s->n_lines]);
}


static void wrap_text(gpointer data, gpointer user_data)
{
	struct renderstuff *s = user_data;
	PangoItem *item = data;
	PangoGlyphString *glyphs;
	PangoLogAttr *log_attrs;
	PangoGlyphItem gitem;
	int *log_widths;
	int width_remain;
	int width_used;
	int i, pos, n;
	const char *ptr;

	log_attrs = calloc(item->num_chars+1, sizeof(PangoLogAttr));
	if ( log_attrs == NULL ) {
		fprintf(stderr, "Failed to allocate memory for log attrs\n");
		return;
	}

	log_widths = calloc(item->num_chars+1, sizeof(int));
	if ( log_widths == NULL ) {
		fprintf(stderr, "Failed to allocate memory for log widths\n");
		return;
	}

	pango_get_log_attrs(s->cur_text, s->cur_len, -1,
	                    pango_language_get_default(),
	                    log_attrs, item->num_chars+1);

	glyphs = pango_glyph_string_new();
	pango_shape(s->cur_text+item->offset, item->length, &item->analysis,
	            glyphs);

	pango_glyph_string_get_logical_widths(glyphs, s->cur_text+item->offset,
	                                      item->length, 0, log_widths);
	gitem.glyphs = glyphs;
	gitem.item = item;

	/* FIXME: Replace this with a real typesetting algorithm */
	width_remain = s->wrap_w*PANGO_SCALE - s->lines[s->n_lines].width;
	width_used = 0;
	pos = 0;
	n = item->num_chars;
	ptr = s->cur_text;
	for ( i=0; i<n; i++ ) {

		if ( !log_attrs[i].is_char_break ) continue;

		if ( log_attrs[i].is_mandatory_break
		 || (log_widths[i] + width_used > width_remain) ) {

			ptr = add_chars_to_line(s, &gitem, pos, ptr);

			/* New line */
			dispatch_line(s);
			width_remain = s->wrap_w * PANGO_SCALE;
			width_used = 0;
			pos = 0;
			printf("Remaining: '%s'\n", ptr);

		} else {
			width_used += log_widths[i];
		}

		pos++;

	}
	ptr = add_chars_to_line(s, &gitem, pos, ptr);

	for ( i=0; i<s->n_lines; i++ ) {
		printf("Line %2i, width = %i\n", i, s->lines[i].width);
	}

	/* Don't dispatch the last line, because the next item might add
	 * more text to it, or the next SC block might add something else. */

	free(log_widths);
	free(log_attrs);
}


static void process_sc_block(struct renderstuff *s, const char *sc_name,
                             const char *sc_options, const char *sc_contents)
{
	size_t len;
	GList *item_list;

	/* Only process textual blocks, for now */
	if ( sc_name != NULL ) return;
	if ( sc_options != NULL ) {
		fprintf(stderr, "Block has options.  WTF?\n");
		return;
	}

	/* Empty block? */
	if ( sc_contents == NULL ) return;

	len = strlen(sc_contents);
	if ( len == 0 ) return;

	/* Create glyph string */
	item_list = pango_itemize(s->pc, sc_contents, 0, len, s->attrs, NULL);
	s->cur_text = sc_contents;
	s->cur_len = len;
	g_list_foreach(item_list, wrap_text, s);

	g_list_free(item_list);
}


static double render_glyph_box(cairo_t *cr, struct wrap_box *box)
{
	double box_w;
	if ( box->glyph_item == NULL ) {
		fprintf(stderr, "Box %p has NULL pointer.\n", box);
		return 0.0;
	}
	pango_cairo_show_glyph_item(cr, box->text, box->glyph_item);
	box_w = pango_glyph_string_get_width(box->glyph_item->glyphs);
	return box_w;
}


static void render_boxes(struct wrap_line *line, struct renderstuff *s)
{
	int j;
	double x_pos = 0.0;

	for ( j=0; j<line->n_boxes; j++ ) {

		struct wrap_box *box;

		cairo_save(s->cr);

		box = &line->boxes[j];
		cairo_rel_move_to(s->cr, x_pos, 0.0);

		switch ( line->boxes[j].type ) {

			case WRAP_BOX_PANGO :
			render_glyph_box(s->cr, box);
			break;

		}

		x_pos += (double)line->boxes[j].width / PANGO_SCALE;

		cairo_restore(s->cr);

	}
}


static void render_lines(struct renderstuff *s)
{
	int i;

	cairo_set_source_rgba(s->cr, 0.4, 0.0, 0.7, 1.0);
	for ( i=0; i<s->n_lines; i++ ) {

		double spacing = 10.0;
		double ascent = 10.0;

		/* Move to beginning of the line */
		cairo_move_to(s->cr, 0.0, ascent + i*spacing);

		/* Render the line */
		render_boxes(&s->lines[i], s);

	}
}


/* Render Level 1 Storycode (no subframes) */
static int render_sc(struct frame *fr, double max_w, double max_h)
{
	PangoFontDescription *fontdesc;
	struct renderstuff s;
	double w, h;
	PangoAttribute *attr_font;
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;
	int i;

	if ( fr->sc == NULL ) return 0;

	bl = sc_find_blocks(fr->sc, NULL);

	if ( bl == NULL ) {
		printf("Failed to find blocks.\n");
		return 0;
	}

	s.wrap_w = max_w;
	s.lines = NULL;
	s.n_lines = 0;
	s.max_lines = 64;
	alloc_lines(&s);

	/* Find and load font */
	s.fontmap = pango_cairo_font_map_get_default();
	s.pc = pango_font_map_create_context(s.fontmap);
	fontdesc = pango_font_description_from_string("Sorts Mill Goudy Bold 24");

	/* Set up attribute list to use the font */
	s.attrs = pango_attr_list_new();
	attr_font = pango_attr_font_desc_new(fontdesc);
	pango_attr_list_insert_before(s.attrs, attr_font);
	pango_font_description_free(fontdesc);

	/* Iterate through SC blocks and send each one in turn for processing */
	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		process_sc_block(&s, b->name, b->options, b->contents);
	}
	sc_block_list_free(bl);

	w = 0.0;  h = 0.0;
	for ( i=0; i<s.n_lines; i++ ) {
		if ( s.lines[i].width > w ) w = s.lines[i].width;
		h += s.lines[i].height;
	}
	w /= PANGO_SCALE;
	h /= PANGO_SCALE;

	/* Determine width */
	w += fr->lop.pad_l + fr->lop.pad_r;
	if ( fr->lop.use_min_w && (w < fr->lop.min_w) ) {
		w = fr->lop.min_w;
	}
	if ( w > max_w ) w = max_w;

	h += fr->lop.pad_t + fr->lop.pad_b;
	if ( fr->lop.use_min_h && (h < fr->lop.min_h) ) {
		h = fr->lop.min_h;
	}
	if ( h > max_h ) h = max_h;

	/* Having decided on the size, create surface and render the contents */
	if ( fr->contents != NULL ) cairo_surface_destroy(fr->contents);
	fr->contents = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	s.cr = cairo_create(fr->contents);

	cairo_translate(s.cr, -fr->lop.pad_l, -fr->lop.pad_t);
	render_lines(&s);

	cairo_destroy(s.cr);
	pango_attr_list_unref(s.attrs);
	g_object_unref(s.pc);

	fr->w = w;
	fr->h = h;

	return 0;
}


static int get_max_size(struct frame *fr, struct frame *parent,
                         int this_subframe, double fixed_x, double fixed_y,
                         double parent_max_width, double parent_max_height,
                         double *p_width, double *p_height)
{
	double w, h;
	int i;

	w = parent_max_width - parent->lop.pad_l - parent->lop.pad_r;
	w -= fr->lop.margin_l + fr->lop.margin_r;

	h = parent_max_height - parent->lop.pad_t - parent->lop.pad_b;
	h -= fr->lop.margin_t + fr->lop.margin_b;

	for ( i=0; i<this_subframe; i++ ) {

		//struct frame *ch;

		//ch = parent->children[i];

		/* FIXME: Shrink if this frame overlaps */
		switch ( fr->lop.grav )
		{
			case DIR_UL:
			/* Fix top left corner */
			break;

			case DIR_U:
			case DIR_UR:
			case DIR_R:
			case DIR_DR:
			case DIR_D:
			case DIR_DL:
			case DIR_L:
			fprintf(stderr, "Gravity not implemented.\n");
			break;

			case DIR_NONE:
			break;
		}
	}

	*p_width = w;
	*p_height = h;
	return 0;
}



static void position_frame(struct frame *fr, struct frame *parent)
{
	switch ( fr->lop.grav )
	{
		case DIR_UL:
		/* Fix top left corner */
		fr->x = parent->x + parent->lop.pad_l + fr->lop.margin_l;
		fr->y = parent->y + parent->lop.pad_t + fr->lop.margin_t;
		break;

		case DIR_U:
		case DIR_UR:
		case DIR_R:
		case DIR_DR:
		case DIR_D:
		case DIR_DL:
		case DIR_L:
		case DIR_NONE:
		fprintf(stderr, "Gravity not implemented.\n");
		break;

		break;
	}
}


static int render_frame(struct frame *fr, cairo_t *cr,
                        double max_w, double max_h)
{
	if ( fr->num_children > 0 ) {

		int i;

		/* Render all subframes */
		for ( i=0; i<fr->num_children; i++ ) {

			double x, y, child_max_w, child_max_h;
			struct frame *ch = fr->children[i];
			int changed;

			if ( ch->style != NULL ) {
				memcpy(&ch->lop, &ch->style->lop,
				       sizeof(struct layout_parameters));
			}

			get_max_size(ch, fr, i, x, y, max_w, max_h,
				     &child_max_w, &child_max_h);

			do {

				/* Render it and hence (recursively) find out
				 * how much space it actually needs.*/
				render_frame(ch, cr, child_max_w, child_max_h);

				changed = get_max_size(ch, fr, i, x, y,
				                       max_w, max_h,
					               &child_max_w,
					               &child_max_h);

			} while ( changed );

			/* Position the frame within the parent */
			position_frame(ch, fr);

		}

	} else {

		render_sc(fr, max_w, max_h);

	}

	return 0;
}


void recursive_buffer_free(struct frame *fr)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {
		recursive_buffer_free(fr->children[i]);
	}

	if ( fr->contents != NULL ) {
		cairo_surface_destroy(fr->contents);
		fr->contents = NULL;
	}
}


void free_render_buffers(struct slide *s)
{
	recursive_buffer_free(s->top);
	if ( s->rendered_edit != NULL ) cairo_surface_destroy(s->rendered_edit);
	if ( s->rendered_proj != NULL ) cairo_surface_destroy(s->rendered_proj);
	if ( s->rendered_thumb != NULL ) {
		cairo_surface_destroy(s->rendered_thumb);
	}
}


static void do_composite(struct frame *fr, cairo_t *cr)
{
	if ( fr->contents == NULL ) return;

	cairo_rectangle(cr, fr->x, fr->y, fr->w, fr->h);
	cairo_set_source_surface(cr, fr->contents, fr->x, fr->y);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke(cr);
}


static int composite_frames_at_level(struct frame *fr, cairo_t *cr,
                                     int level, int cur_level)
{
	int i;

	if ( level == cur_level ) {

		/* This frame is at the right level, so composite it */
		do_composite(fr, cr);
		return 1;

	} else {

		int n = 0;

		for ( i=0; i<fr->num_children; i++ ) {
			n += composite_frames_at_level(fr->children[i], cr,
			                               level, cur_level+1);
		}

		return n;

	}
}


static void composite_slide(struct slide *s, cairo_t *cr)
{
	int level = 0;
	int more = 0;

	do {
		more = composite_frames_at_level(s->top, cr, level, 0);
		level++;
	} while ( more != 0 );
}


cairo_surface_t *render_slide(struct slide *s, int w, int h)
{
	cairo_surface_t *surf;
	cairo_t *cr;
	cairo_font_options_t *fopts;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

	cr = cairo_create(surf);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	fopts = cairo_font_options_create();
	cairo_font_options_set_hint_style(fopts, CAIRO_HINT_STYLE_NONE);
	cairo_font_options_set_hint_metrics(fopts, CAIRO_HINT_METRICS_OFF);
	cairo_font_options_set_antialias(fopts, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_set_font_options(cr, fopts);

	s->top->lop.min_w = w;
	s->top->lop.min_h = h;
	s->top->lop.use_min_w = 1;
	s->top->lop.use_min_h = 1;
	render_frame(s->top, cr, w, h);

	composite_slide(s, cr);

	cairo_font_options_destroy(fopts);
	cairo_destroy(cr);

	return surf;
}
