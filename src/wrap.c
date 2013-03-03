/*
 * wrap.c
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pango/pangocairo.h>

#include <storycode.h>

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
		if ( box->height > line->height ) line->height = box->height;
		if ( box->ascent > line->ascent ) line->ascent = box->ascent;
	}
}


/* Add "text", followed by a space of type "space", to "line" */
static int add_wrap_box(struct wrap_line *line, char *text,
                        enum wrap_box_space space, PangoContext *pc,
                        PangoFont *font, double col[4])
{
	GList *pango_items;
	struct wrap_box *box;
	PangoAttrList *attrs;

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
	pango_items = pango_itemize(pc, text, 0, strlen(text), attrs, NULL);
	g_list_foreach(pango_items, shape_and_measure, box);
	g_list_free(pango_items);
	pango_attr_list_unref(attrs);

	return 0;
}


static int split_words(struct wrap_line *boxes, PangoContext *pc, char *sc,
                       PangoFont *font, double col[4])
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
	pango_get_log_attrs(sc, len, -1, pango_language_get_default(),
	                    log_attrs, len+1);

	start = 0;
	for ( i=0; i<len; i++ ) {

		if ( log_attrs[i].is_line_break ) {

			char *word;
			enum wrap_box_space type;

			/* Stuff up to (but not including) sc[i] forms a
			 * wap box */
			word = strndup(sc+start, i-start);
			if ( word == NULL ) {
				fprintf(stderr, "strndup() failed.\n");
				free(log_attrs);
				return 1;
			}

			if ( log_attrs[i].is_mandatory_break ) {
				type = WRAP_SPACE_EOP;
			} else if ( log_attrs[i].is_expandable_space ) {
				type = WRAP_SPACE_INTERWORD;
			} else {
				type = WRAP_SPACE_NONE;
			}

			if ( add_wrap_box(boxes, word, type, pc, font, col) ) {
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

		add_wrap_box(boxes, word, WRAP_SPACE_NONE, pc, font, col);

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

	/* FIXME: Determine the proper font to use */
	fontdesc = pango_font_description_from_string("Sorts Mill Goudy 16");
	font = pango_font_map_load_font(pango_context_get_font_map(pc),
	                                pc, fontdesc);
	pango_font_description_free(fontdesc);
	col[0] = 0.0;  col[1] = 0.0;  col[2] = 0.0;  col[3] = 1.0;

	/* Iterate through SC blocks and send each one in turn for processing */
	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		if ( b->name == NULL ) {
			if ( split_words(boxes, pc, b->contents, font, col) ) {
				fprintf(stderr, "Splitting failed.\n");
			}
		}

		/* FIXME: Handle images */
	}
	sc_block_list_free(bl);

	return boxes;
}


/* Wrap the StoryCode inside "fr->sc" so that it fits within width "fr->w",
 * and generate fr->lines */
int wrap_contents(struct frame *fr, PangoContext *pc)
{
	struct wrap_line *boxes;

	/* Turn the StoryCode into wrap boxes, all on one line */
	boxes = sc_to_wrap_boxes(fr->sc, pc);
	if ( boxes == NULL ) {
		fprintf(stderr, "Failed to create wrap boxes.\n");
		return 1;
	}

	/* FIXME: Hack for test purposes */
	calc_line_geometry(boxes);
	fr->lines = boxes;
	fr->n_lines = 1;
	fr->max_lines = 1;

	return 0;
}

