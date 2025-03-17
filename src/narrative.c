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

Narrative *narrative_new()
{
    Narrative *n;
    n = malloc(sizeof(*n));
    if ( n == NULL ) return NULL;
    n->saved = 1;
    n->textbuf = gtk_text_buffer_new(NULL);

    n->n_slides = 0;
    n->slides = malloc(64*sizeof(Slide *));
    n->max_slides = 64;

    gtk_text_buffer_create_tag(n->textbuf, "overall",
                               "wrap-mode", GTK_WRAP_WORD_CHAR,
                               "font", "Sans 16",
                               "left-margin", 10,
                               "right-margin", 10,
                               "pixels-above-lines", 10,
                               "background", "#ffffff",
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "text", NULL);

    gtk_text_buffer_create_tag(n->textbuf, "segstart",
                               "font", "Sans Bold 18",
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "prestitle",
                               "font", "Sans Bold 24",
                               NULL);

    gtk_text_buffer_create_tag(n->textbuf, "bulletpoint",
                               "indent", 20,
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


#if 0
static size_t write_string(GOutputStream *fh, char *str)
{
    gssize r;
    GError *error = NULL;
    r = g_output_stream_write(fh, str, strlen(str), NULL, &error);
    if ( r == -1 ) {
        fprintf(stderr, "Write failed: %s\n", error->message);
        return 0;
    }
    return strlen(str);
}


static size_t write_run_border(GOutputStream *fh, enum text_run_type t)
{
    if ( t == TEXT_RUN_BOLD ) return write_string(fh, "**");
    if ( t == TEXT_RUN_ITALIC ) return write_string(fh, "*");
    if ( t == TEXT_RUN_UNDERLINE ) return write_string(fh, "_");
    return 0;
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


static size_t write_partial_run(GOutputStream *fh,
                                struct text_run *run,
                                size_t start,
                                size_t len)
{
    char *escaped_str;
    size_t tlen = 0;
    tlen += write_run_border(fh, run->type);
    escaped_str = escape_text(run->text+start, len);
    tlen += write_string(fh, escaped_str);
    free(escaped_str);
    tlen += write_run_border(fh, run->type);
    return tlen;
}


static size_t write_para(GOutputStream *fh, struct text_run *runs, int n_runs)
{
    int i;
    size_t len = 0;
    for ( i=0; i<n_runs; i++ ) {
        if ( strlen(runs[i].text) == 0 ) continue;
        len += write_partial_run(fh, &runs[i], 0, strlen(runs[i].text));
    }
    return len;
}


static void write_series(GOutputStream *fh, char *c, int n)
{
    int i;
    for ( i=0; i<n; i++ ) {
        write_string(fh, c);
    }
}


static int write_markdown(GOutputStream *fh, Narrative *n)
{
    int i;
    for ( i=0; i<n->n_items; i++ ) {

        char tmp[64];
        size_t len;
        struct narrative_item *item = &n->items[i];

        if ( i != 0 )  write_string(fh, "\n");

        switch ( item->type ) {

            case NARRATIVE_ITEM_TEXT:
            write_para(fh, item->runs, item->n_runs);
            write_string(fh, "\n");
            break;

            case NARRATIVE_ITEM_BP:
            write_string(fh, "* ");
            write_para(fh, item->runs, item->n_runs);
            write_string(fh, "\n\n");
            break;

            case NARRATIVE_ITEM_SEGSTART:
            write_string(fh, "\n");
            len = write_para(fh, item->runs, item->n_runs);
            write_string(fh, "\n");
            write_series(fh, "-", len);
            write_string(fh, "\n");
            break;

            case NARRATIVE_ITEM_PRESTITLE:
            len = write_para(fh, item->runs, item->n_runs);
            write_string(fh, "\n");
            write_series(fh, "=", len);
            write_string(fh, "\n");
            break;

            case NARRATIVE_ITEM_SLIDE:
            write_string(fh, "###### Slide ");
            snprintf(tmp, 64, "%i", item->slide->ext_slidenumber);
            write_string(fh, tmp);
            write_string(fh, "; ");
            write_string(fh, item->slide->ext_filename);
            write_string(fh, "\n");
            break;

            case NARRATIVE_ITEM_SEGEND:
            case NARRATIVE_ITEM_EOP:
            break;
        }
    }

    return 0;
}
#endif


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
    r = 1;//write_markdown(G_OUTPUT_STREAM(fh), n);  FIXME!
    if ( r ) {
        fprintf(stderr, _("Couldn't save presentation\n"));
    }
    g_object_unref(fh);

    n->saved = 1;
    return 0;
}


/* Find the position (in units of narrative items) of time 'minutes' */
double narrative_find_time_pos(Narrative *n, double minutes)
{
    return 0;
/* FIXME: Word count */
#if 0
    int i;
    double t = 0.0;

    if ( minutes == 0.0 ) return 0.0;

    for ( i=0; i<n->n_items; i++ ) {
        double idur = n->items[i].estd_duration;
        if ( t + idur > minutes ) {
            /* It's in this item */
            return (double)i + (minutes - t)/idur;
        }
        t += n->items[i].estd_duration;
    }
    return n->n_items;
#endif
}


/* FIXME: Eventually, there should be no need for constant slide size */
void narrative_get_first_slide_size(Narrative *n, double *w, double *h)
{
    if ( n->n_slides == 0 ) {
        printf("No slides found - using standard slide size\n");
        *w = 1024.0;  *h = 768.0;
    } else {
        slide_get_logical_size(n->slides[0], w, h);
    }
}


#ifdef HAVE_MD4C

struct md_parse_ctx {
    Narrative *n;
    enum narrative_item_type type;
    int bold;
    int italic;
    int underline;
};


static void close_block(struct md_parse_ctx *ps)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(ps->n->textbuf, &end);
    gtk_text_buffer_insert(ps->n->textbuf, &end, "\n", -1);
}


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
    close_block(ps);
    return 0;
}


static const char *block_tag_name(struct md_parse_ctx *ps)
{
    switch ( ps->type ) {
        case NARRATIVE_ITEM_TEXT : return "text";
        case NARRATIVE_ITEM_SEGSTART : return "segstart";
        case NARRATIVE_ITEM_PRESTITLE : return "prestitle";
        case NARRATIVE_ITEM_BP : return "bulletpoint";
        default: return "text";
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


static int get_ext_slide_size(Slide *s, double *pw, double *ph)
{
    GFile *file;
    PopplerDocument *doc;
    PopplerPage *page;

    file = g_file_new_for_path(s->ext_filename);
    doc = poppler_document_new_from_gfile(file, NULL, NULL, NULL);
    if ( doc == NULL ) return 1;

    page = poppler_document_get_page(doc, s->ext_slidenumber-1);
    if ( page == NULL ) return 1;

    poppler_page_get_size(page, pw, ph);

    g_object_unref(G_OBJECT(page));
    g_object_unref(G_OBJECT(doc));

    return 0;
}

static int md_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE len, void *vp)
{
    struct md_parse_ctx *ps = vp;

    if ( ps->type == NARRATIVE_ITEM_SLIDE ) {

        double w, h;

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
        get_ext_slide_size(slide, &w, &h);
        slide_set_logical_size(slide, w, h);

        GtkTextIter start;
        gtk_text_buffer_get_end_iter(ps->n->textbuf, &start);
        slide->anchor = gtk_text_buffer_create_child_anchor(ps->n->textbuf, &start);

        free(tx);

    } else {

        GtkTextIter start_iter, end_iter;
        int start_offset;

        gtk_text_buffer_get_end_iter(ps->n->textbuf, &end_iter);
        start_offset = gtk_text_iter_get_offset(&end_iter);

        gtk_text_buffer_insert_with_tags_by_name(ps->n->textbuf, &end_iter, text, len,
                                                 block_tag_name(ps), NULL);

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

    md_parse(text, len, &md_parser, &pstate);

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

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(n->textbuf, &start, &end);
    gtk_text_buffer_apply_tag_by_name(n->textbuf, "overall", &start, &end);

    return n;
}
