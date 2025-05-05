/*
 * narrative.c
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <poppler.h>

#include <libintl.h>
#define _(x) gettext(x)

#ifdef HAVE_MD4C
#include <md4c.h>
#endif

#include "slide.h"
#include "narrative.h"
#include "thumbnailwidget.h"


Narrative *narrative_new()
{
    Narrative *n;
    n = malloc(sizeof(*n));
    if ( n == NULL ) return NULL;
    n->textbuf = gtk_text_buffer_new(NULL);

    n->n_slides = 0;
    n->slides = malloc(64*sizeof(Slide *));
    n->max_slides = 64;

    n->n_time_marks = 1;
    n->time_marks = malloc(60*sizeof(struct time_mark));
    n->time_marks[0].minutes = 1.0;
    n->time_marks[0].y = 300.0;

    gtk_text_buffer_create_tag(n->textbuf, "segstart",
                               "font", "Sans Bold 18",
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "prestitle",
                               "font", "Sans Bold 24",
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "bulletpoint",
                               "indent", 20,
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "slide",
                               "background", "#ddddff",
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "bold",
                               "weight", PANGO_WEIGHT_BOLD,
                               NULL);
    gtk_text_buffer_create_tag(n->textbuf, "italic",
                               "style", PANGO_STYLE_ITALIC,
                               NULL);
    gtk_text_buffer_create_tag(n->textbuf, "underline",
                               "underline", PANGO_UNDERLINE_SINGLE,
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "highlight",
                               "background", "#ddddff",
                               "background-full-height", TRUE,
                               NULL);

    gtk_text_buffer_set_modified(n->textbuf, FALSE);

    return n;
}


/* Free the narrative and all contents */
void narrative_free(Narrative *n)
{
    g_object_unref(n->textbuf);
    free(n);
}


Slide *narrative_get_slide(Narrative *n, int para)
{
    return n->slides[0];
}


static ssize_t write_string(GOutputStream *fh, char *str)
{
    gssize r;
    GError *error = NULL;
    r = g_output_stream_write(fh, str, strlen(str), NULL, &error);
    if ( r == -1 ) {
        fprintf(stderr, "Write failed: %s\n", error->message);
        return -1;
    }
    return strlen(str);
}


static void write_series(GOutputStream *fh, char *c, int n)
{
    int i;
    for ( i=0; i<n; i++ ) {
        write_string(fh, c);
    }
}


#define TB_BOLD (1<<0)
#define TB_ITALIC (1<<1)
#define TB_UNDERLINE (1<<2)

static void set_bit(int *bp, int bit)
{
    *bp = (*bp) | bit;
}


static void clear_bit(int *bp, int bit)
{
    *bp = ((*bp) | bit) ^ bit;
}


static void write_tag_start(GOutputStream *fh,
                            GtkTextTag *tag,
                            GtkTextIter *iter,
                            int *char_count,
                            int *tags_open)
{
    gchar *name;
    g_object_get(G_OBJECT(tag), "name", &name, NULL);
    if ( strcmp(name, "bold") == 0 ) {
        write_string(fh, "**");
        set_bit(tags_open, TB_BOLD);
    }
    if ( strcmp(name, "italic") == 0 ) {
        write_string(fh, "*");
        set_bit(tags_open, TB_ITALIC);
    }
    if ( strcmp(name, "underline") == 0 ) {
        write_string(fh, "_");
        set_bit(tags_open, TB_UNDERLINE);
    }
    if ( strcmp(name, "segstart") == 0 ) {
        write_string(fh, "\n");
        *char_count = 0;
    }
    if ( strcmp(name, "prestitle") == 0 ) {
        *char_count = 0;
    }
    if ( strcmp(name, "bulletpoint") == 0 ) {
        write_string(fh, "* ");
    }
    if ( strcmp(name, "slide") == 0 ) {
        GtkTextChildAnchor *anc = gtk_text_iter_get_child_anchor(iter);
        if ( anc != NULL ) {
            GtkWidget **widgets;
            Slide *slide;
            guint len;
            char tmp[64];
            widgets = gtk_text_child_anchor_get_widgets(anc, &len);
            assert(len == 1);
            slide = thumbnail_get_slide(COLLOQUIUM_THUMBNAIL(widgets[0]));
            write_string(fh, "###### Slide ");
            snprintf(tmp, 64, "%i", slide->ext_slidenumber);
            write_string(fh, tmp);
            write_string(fh, "; ");
            write_string(fh, slide->ext_filename);
        }
    }
    g_free(name);
}


static void write_tag_end(GOutputStream *fh, GtkTextTag *tag, int *last_len, int *tags_open)
{
    gchar *name;
    g_object_get(G_OBJECT(tag), "name", &name, NULL);
    if ( (strcmp(name, "bold") == 0) && (*tags_open & TB_BOLD) ) {
        write_string(fh, "**");
        clear_bit(tags_open, TB_BOLD);
    }
    if ( (strcmp(name, "italic") == 0) && (*tags_open & TB_ITALIC) ) {
        write_string(fh, "*");
        clear_bit(tags_open, TB_ITALIC);
    }
    if ( (strcmp(name, "underline") == 0) && (*tags_open & TB_UNDERLINE) ) {
        write_string(fh, "_");
        clear_bit(tags_open, TB_UNDERLINE);
    }
    if ( strcmp(name, "prestitle") == 0 ) {
        write_string(fh, "\n");
        write_series(fh, "=", *last_len);
        *last_len = 0;
    }
    if ( strcmp(name, "segstart") == 0 ) {
        write_string(fh, "\n");
        write_series(fh, "-", *last_len);
        *last_len = 0;
    }
    g_free(name);
}


static void close_span_tags(GOutputStream *fh, int *tags)
{
    if ( *tags & TB_BOLD ) {
        write_string(fh, "**");
        clear_bit(tags, TB_BOLD);
    }
    if ( *tags & TB_ITALIC ) {
        write_string(fh, "*");
        clear_bit(tags, TB_ITALIC);
    }
    if ( *tags & TB_UNDERLINE ) {
        write_string(fh, "_");
        clear_bit(tags, TB_UNDERLINE);
    }
}


static char *escape_text(const char *in, size_t len)
{
    int i, j;
    size_t nl = 0;
    size_t np = 0;
    const char *esc = "*/_";
    size_t n_esc = 3;
    char *out;

    for ( i=0; i<len; i++ ) {
        for ( j=0; j<n_esc; j++ ) {
            if ( in[i] == esc[j] ) {
                nl++;
                break;
            }
        }
    }

    out = malloc(len + nl + 1);
    if ( out == NULL ) return NULL;

    np = 0;
    for ( i=0; i<len; i++ ) {
        for ( j=0; j<n_esc; j++ ) {
            if ( in[i] == esc[j] ) {
                out[np++] = '\\';
                break;
            }
        }
        out[np++] = in[i];
    }
    out[np] = '\0';

    return out;
}


static int write_escaped_text(GOutputStream *fh, char *str)
{
    size_t len = strlen(str);
    int char_len = g_utf8_strlen(str, len);
    char *esc = escape_text(str, len);
    if ( write_string(fh, esc) == -1 ) return 0;
    free(esc);
    g_free(str);
    return char_len;
}


static void write_tags(GtkTextIter pos, int *char_count, GOutputStream *fh, int *tags_open)
{
    GSList *tag;

    for ( tag=gtk_text_iter_get_toggled_tags(&pos, FALSE);
          tag != NULL;
          tag=g_slist_next(tag) )
    {
        write_tag_end(fh, tag->data, char_count, tags_open);
    }

    for ( tag=gtk_text_iter_get_toggled_tags(&pos, TRUE);
          tag != NULL;
          tag=g_slist_next(tag) )
    {
        write_tag_start(fh, tag->data, &pos, char_count, tags_open);
    }
}


static int write_markdown(GOutputStream *fh, Narrative *n)
{
    GtkTextIter start, end;
    int finished = 0;
    int char_count = 0;
    int tags_open = 0;

    gtk_text_buffer_get_start_iter(n->textbuf, &start);
    gtk_text_buffer_get_end_iter(n->textbuf, &end);

    write_tags(start, &char_count, fh, &tags_open);
    do {

        GtkTextIter end_tag, end_para;
        GtkTextIter nrl;  /* "Next Relevant Location */
        int have_etag, have_epara;
        int epara = 0;

        end_tag = start;
        end_para = start;
        have_etag = gtk_text_iter_forward_to_tag_toggle(&end_tag, NULL);
        have_epara = gtk_text_iter_forward_to_line_end(&end_para);
        if ( !have_etag && !have_epara ) {
            nrl = end;
            finished = 1;
        } else {
            int r;
            r = gtk_text_iter_compare(&end_tag, &end_para);
            if ( r < 0 ) {
                /* end_tag comes first */
                nrl = end_tag;
            } else {
                /* end_para comes first, or they coincide */
                nrl = end_para;
                epara = 1;
            }
        }

        char *str = gtk_text_buffer_get_text(n->textbuf, &start, &nrl, TRUE);
        char_count += write_escaped_text(fh, str);

        write_tags(nrl, &char_count, fh, &tags_open);

        if ( epara ) {

            /* Close any open span tags (bold/italic/underline) */
            close_span_tags(fh, &tags_open);

            /* Sneakily move to start of the next paragraph */
            if ( gtk_text_iter_forward_line(&nrl) ) {
                write_string(fh, "\n\n");
                write_tags(nrl, &char_count, fh, &tags_open);
            } /* else no more lines */

        }

        start = nrl;

    } while ( !finished );
    write_string(fh, "\n");

    return 0;
}


int narrative_save(Narrative *n, GFile *file)
{
    GFileOutputStream *fh;
    int r;
    GError *error = NULL;

    if ( file == NULL ) {
        fprintf(stderr, "Saving to NULL!\n");
        return 1;
    }

    fh = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
    if ( fh == NULL ) {
        fprintf(stderr, _("Open failed: %s\n"), error->message);
        return 1;
    }
    r = write_markdown(G_OUTPUT_STREAM(fh), n);
    if ( r ) {
        fprintf(stderr, _("Couldn't save presentation\n"));
    }
    g_object_unref(fh);

    gtk_text_buffer_set_modified(n->textbuf, FALSE);
    return 0;
}


static int wordcount(char *text)
{
       int j;
       int words = 0;
       size_t len = strlen(text);
       for ( j=0; j<len; j++ ) {
           if ( text[j] == ' ' ) words++;
       }
       return words;
}


void narrative_update_timing(GtkTextView *nv, Narrative *n)
{
    GtkTextIter start;
    int i = 0;
    double total_minutes = 0.0;
    double last_mark = 0.0;

    n->n_time_marks = 0;
    gtk_text_buffer_get_start_iter(n->textbuf, &start);
    do {
        int y, h;
        char *txt;
        GtkTextIter end = start;
        gtk_text_iter_forward_line(&end);
        gtk_text_view_get_line_yrange(nv, &start, &y, &h);
        txt = gtk_text_iter_get_slice(&start, &end);
        total_minutes += wordcount(txt)/120.0;
        g_free(txt);

        if ( total_minutes - last_mark >= 1.0 ) {
            n->time_marks[i].y = y;
            n->time_marks[i].minutes = total_minutes;
            i++;
            if ( i == 60 ) break; /* Lazy memory "management" */
            last_mark = total_minutes;
        }
        start = end;
    } while ( !gtk_text_iter_is_end(&start) );
    n->n_time_marks = i;
}


#ifdef HAVE_MD4C

struct md_parse_ctx {
    Narrative *n;
    enum narrative_item_type type;
    int bold;
    int italic;
    int underline;
    int need_newline;
};


static int md_enter_block(MD_BLOCKTYPE type, void *detail, void *vp)
{
    struct md_parse_ctx *ps = vp;
    MD_BLOCK_H_DETAIL *d;

    switch ( type ) {

        case MD_BLOCK_H:
        d = detail;
        if ( d->level == 1 ) {
            ps->type = NARRATIVE_ITEM_PRESTITLE;
        } else if ( d->level == 6 ) {
            ps->type = NARRATIVE_ITEM_SLIDE;
        } else {
            ps->type = NARRATIVE_ITEM_SEGSTART;
        }
        break;

        case MD_BLOCK_LI:
        ps->type = NARRATIVE_ITEM_BP;
        break;

        default :
        ps->type = NARRATIVE_ITEM_TEXT;
        break;

    }

    return 0;
}


static int md_leave_block(MD_BLOCKTYPE type, void *detail, void *vp)
{
    struct md_parse_ctx *ps = vp;
    if ( type == MD_BLOCK_DOC ) return 0;
    ps->need_newline = 1;
    return 0;
}


static const char *block_tag_name(struct md_parse_ctx *ps)
{
    switch ( ps->type ) {
        case NARRATIVE_ITEM_TEXT : return NULL;
        case NARRATIVE_ITEM_SEGSTART : return "segstart";
        case NARRATIVE_ITEM_PRESTITLE : return "prestitle";
        case NARRATIVE_ITEM_BP : return "bulletpoint";
        default: return NULL;
    }
}


static int md_enter_span(MD_SPANTYPE type, void *detail, void *vp)
{
    struct md_parse_ctx *ps = vp;
    if ( type == MD_SPAN_STRONG ) { ps->bold++; }
    if ( type == MD_SPAN_EM ) { ps->italic++; }
    if ( type == MD_SPAN_U ) { ps->underline++; }
    return 0;
}


static int md_leave_span(MD_SPANTYPE type, void *detail, void *vp)
{
    struct md_parse_ctx *ps = vp;
    if ( type == MD_SPAN_STRONG ) { ps->bold--; }
    if ( type == MD_SPAN_EM ) { ps->italic--; }
    if ( type == MD_SPAN_U ) { ps->underline--; }
    return 0;
}


void insert_slide_anchor(GtkTextBuffer *buf, Slide *slide, GtkTextIter start, int newline)
{
    GtkTextIter end;
    GtkTextMark *mark;

    /* Mark this position for later */
    mark = gtk_text_mark_new(NULL, TRUE);
    gtk_text_buffer_add_mark(buf, mark, &start);

    /* Insert the slide's anchor */
    slide->anchor = gtk_text_buffer_create_child_anchor(buf, &start);

    /* Retrieve the mark  and figure out positions before and after the slide */
    gtk_text_buffer_get_iter_at_mark(buf, &end, mark);
    start = end;
    gtk_text_iter_forward_cursor_position(&end);
    gtk_text_buffer_apply_tag_by_name(buf, "slide", &start, &end);

    /* Retrieve the mark (again) and add a newline, if requested */
    if ( newline ) {
        gtk_text_buffer_get_iter_at_mark(buf, &end, mark);
        gtk_text_iter_forward_cursor_position(&end);
        gtk_text_buffer_insert(buf, &end, "\n", 1);
    }

    /* Mark is not needed any more */
    gtk_text_buffer_delete_mark(buf, mark);
}


static int md_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE len, void *vp)
{
    struct md_parse_ctx *ps = vp;

    if ( ps->need_newline ) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(ps->n->textbuf, &end);
        gtk_text_buffer_insert(ps->n->textbuf, &end, "\n", -1);
        ps->need_newline = 0;
    }

    if ( ps->type == NARRATIVE_ITEM_SLIDE ) {

        if ( strncmp(text, "Slide ", 6) != 0 ) return 1;
        char *tx = strndup(text+6, len-6);
        const char *sc = strchr(tx, ';');
        if ( sc == NULL ) return 1;
        if ( strlen(sc) < 3 ) return 1;

        Slide *slide = slide_new();
        if ( ps->n->n_slides == ps->n->max_slides ) {
            int nmax_slides = ps->n->max_slides*2;
            Slide **nslides = realloc(ps->n->slides, nmax_slides*sizeof(Slide *));
            if ( nslides == NULL ) {
                fprintf(stderr, "Failed to add slide\n");
                return 1;
            }
            ps->n->max_slides = nmax_slides;
            ps->n->slides = nslides;
        }
        ps->n->slides[ps->n->n_slides++] = slide;

        slide_set_ext_filename(slide, strdup(sc+2));
        slide_set_ext_number(slide, atoi(tx));

        GtkTextIter start;
        gtk_text_buffer_get_end_iter(ps->n->textbuf, &start);
        insert_slide_anchor(ps->n->textbuf, slide, start, 0);

        free(tx);

    } else {

        GtkTextIter start_iter, end_iter;
        int start_offset;

        gtk_text_buffer_get_end_iter(ps->n->textbuf, &end_iter);
        start_offset = gtk_text_iter_get_offset(&end_iter);

        const char *tn = block_tag_name(ps);
        if ( tn == NULL ) {
            gtk_text_buffer_insert(ps->n->textbuf, &end_iter, text, len);
        } else {
            gtk_text_buffer_insert_with_tags_by_name(ps->n->textbuf, &end_iter, text, len,
                                                     block_tag_name(ps), NULL);
        }

        gtk_text_buffer_get_iter_at_offset(ps->n->textbuf, &start_iter, start_offset);

        if ( ps->bold ) {
            gtk_text_buffer_apply_tag_by_name(ps->n->textbuf, "bold", &start_iter, &end_iter);
        }
        if ( ps->italic ) {
            gtk_text_buffer_apply_tag_by_name(ps->n->textbuf, "italic", &start_iter, &end_iter);
        }
        if ( ps->underline ) {
            gtk_text_buffer_apply_tag_by_name(ps->n->textbuf, "underline", &start_iter, &end_iter);
        }
    }

    return 0;
}


static void md_debug_log(const char *msg, void *vp)
{
    printf("%s\n", msg);
}


const struct MD_PARSER md_parser =
{
    .abi_version = 0,
    .flags = MD_FLAG_UNDERLINE,
    .enter_block = md_enter_block,
    .leave_block = md_leave_block,
    .enter_span = md_enter_span,
    .leave_span = md_leave_span,
    .text = md_text,
    .debug_log = md_debug_log,
    .syntax = NULL
};


static Narrative *parse_md_narrative(const char *text, size_t len)
{
    struct md_parse_ctx pstate;

    pstate.n = narrative_new();
    pstate.bold = 0;
    pstate.italic = 0;
    pstate.underline = 0;
    pstate.type = NARRATIVE_ITEM_TEXT;
    pstate.need_newline = 0;

    gtk_text_buffer_begin_irreversible_action(pstate.n->textbuf);
    md_parse(text, len, &md_parser, &pstate);
    gtk_text_buffer_end_irreversible_action(pstate.n->textbuf);

    return pstate.n;
}

#else  // HAVE_MD4C

static Narrative *parse_md_narrative(const char *text, size_t len)
{
    return NULL;
}

#endif


Narrative *narrative_load(GFile *file)
{
    GBytes *bytes;
    const char *text;
    size_t len;
    Narrative *n;

    bytes = g_file_load_bytes(file, NULL, NULL, NULL);
    if ( bytes == NULL ) return NULL;

    text = g_bytes_get_data(bytes, &len);
    n = parse_md_narrative(text, len);
    g_bytes_unref(bytes);
    if ( n == NULL ) return NULL;

    return n;
}
