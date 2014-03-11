/*
 * shape.c
 *
 * Copyright © 2014 Thomas White <taw@bitwiz.org.uk>
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


static void shape_and_measure(gpointer data, gpointer user_data)
{
	struct wrap_box *box = user_data;
	PangoItem *item = data;
	PangoRectangle rect;

	/* FIXME: Don't assume only one run per wrap box */
	box->glyphs = pango_glyph_string_new();
	box->item = item;

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
}


/* Add "text", followed by a space of type "space", to "line" */
static int add_wrap_box(struct wrap_line *line, char *text, size_t offset,
                        enum wrap_box_space space, PangoContext *pc,
                        SCInterpreter *scin, int editable)
{
	GList *pango_items;
	struct wrap_box *box;
	PangoAttrList *attrs;
	PangoAttribute *attr;
	double *col;

	if ( line->n_boxes == line->max_boxes ) {
		line->max_boxes += 32;
		alloc_boxes(line);
		if ( line->n_boxes == line->max_boxes ) return 1;
	}

	box = &line->boxes[line->n_boxes];
	if ( !editable ) offset = 0;
	box->type = WRAP_BOX_PANGO;
	box->text = text;
	box->space = space;
	box->font = sc_interp_get_font(scin);
	box->width = 0;
	box->editable = editable;
	box->ascent = sc_interp_get_ascent(scin);
	box->height = sc_interp_get_height(scin);
	col = sc_interp_get_fgcol(scin);
	box->col[0] = col[0];  /* Red */
	box->col[1] = col[1];  /* Green */
	box->col[2] = col[2];  /* Blue */
	box->col[3] = col[3];  /* Alpha */
	line->n_boxes++;

	if ( strlen(text) == 0 ) {
		box->type = WRAP_BOX_NOTHING;
		return 0;
	}

	attrs = pango_attr_list_new();
	attr = pango_attr_font_desc_new(sc_interp_get_fontdesc(scin));
	pango_attr_list_insert_before(attrs, attr);
	pango_items = pango_itemize(pc, text, 0, strlen(text), attrs, NULL);
	g_list_foreach(pango_items, shape_and_measure, box);
	g_list_free(pango_items);
	pango_attr_list_unref(attrs);

	return 0;
}


void add_image_box(struct wrap_line *line, const char *filename,
                   int w, int h, int editable)
{
	struct wrap_box *box;

	box = &line->boxes[line->n_boxes];
	box->type = WRAP_BOX_IMAGE;
	box->text = NULL;
	box->space = WRAP_SPACE_NONE;
	box->width = pango_units_from_double(w);
	box->ascent = pango_units_from_double(h);
	box->height = pango_units_from_double(h);
	box->filename = strdup(filename);
	box->editable = editable;
	line->n_boxes++;
}


int split_words(struct wrap_line *boxes, PangoContext *pc, const char *text,
                PangoLanguage *lang, int editable, SCInterpreter *scin)
{
	PangoLogAttr *log_attrs;
	glong len_chars, i;
	size_t len_bytes, start;

	/* Empty block? */
	if ( text == NULL ) return 1;

	len_chars = g_utf8_strlen(text, -1);
	if ( len_chars == 0 ) return 1;

	len_bytes = strlen(text);

	log_attrs = malloc((len_chars+1)*sizeof(PangoLogAttr));
	if ( log_attrs == NULL ) return 1;

	/* Create glyph string */
	pango_get_log_attrs(text, len_bytes, -1, lang, log_attrs, len_chars+1);

	start = 0;
	for ( i=0; i<len_chars; i++ ) {

		if ( log_attrs[i].is_line_break ) {

			char *word;
			enum wrap_box_space type;
			size_t len;
			char *ptr;
			size_t offs;

			ptr = g_utf8_offset_to_pointer(text, i);
			offs = ptr - text;

			/* Stuff up to (but not including) sc[i] forms a
			 * wrap box */
			len = offs - start;
			if ( log_attrs[i].is_mandatory_break ) {
				type = WRAP_SPACE_EOP;
				if ( (offs>0) && (text[offs-1] == '\n') ) len--;
				if ( (offs>0) && (text[offs-1] == '\r') ) len--;
			} else if ( (i>0)
			         && log_attrs[i-1].is_expandable_space ) {
				type = WRAP_SPACE_INTERWORD;
				len--;  /* Not interested in spaces */
			} else {
				type = WRAP_SPACE_NONE;
			}

			word = strndup(text+start, len);
			if ( word == NULL ) {
				fprintf(stderr, "strndup() failed.\n");
				free(log_attrs);
				return 1;
			}

			if ( add_wrap_box(boxes, word, start, type,
			                  pc, scin, editable) ) {
				fprintf(stderr, "Failed to add wrap box.\n");
			}
			start = offs;

		}

	}
	if ( i > start ) {

		char *word;
		size_t l;

		word = strdup(text+start);  /* to the end */
		if ( word == NULL ) {
			fprintf(stderr, "strndup() failed.\n");
			free(log_attrs);
			return 1;
		}
		l = strlen(word);

		if ( (word[l-1] == '\n')  ) {

			/* There is a newline at the end of the SC */
			char *word2;

			word2 = strndup(word, l-1);
			add_wrap_box(boxes, word2, start,
			             WRAP_SPACE_EOP, pc, scin, editable);
			add_wrap_box(boxes, strdup(""), len_bytes,
			             WRAP_SPACE_NONE, pc, scin, editable);

		} else {

			add_wrap_box(boxes, word, start,
			             WRAP_SPACE_NONE, pc, scin, editable);

		}

	}

	free(log_attrs);
	return 0;
}
