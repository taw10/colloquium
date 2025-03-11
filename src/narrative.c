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

#ifdef HAVE_PANGO
#include <pango/pango.h>
#endif

#ifdef HAVE_CAIRO
#include <cairo.h>
#endif

#ifdef HAVE_MD4C
#include <md4c.h>
#endif

#include "slide.h"
#include "narrative.h"
#include "narrative_priv.h"

Narrative *narrative_new()
{
    Narrative *n;
    n = malloc(sizeof(*n));
    if ( n == NULL ) return NULL;
    n->n_items = 0;
    n->items = NULL;
    n->saved = 1;
    n->textbuf = gtk_text_buffer_new(NULL);
#ifdef HAVE_PANGO
    n->language = pango_language_to_string(pango_language_get_default());
#else
    n->language = NULL;
#endif

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

    gtk_text_buffer_create_tag(n->textbuf, "normal",
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


static void narrative_item_destroy(struct narrative_item *item)
{
    int i;
    for ( i=0; i<item->n_runs; i++ ) {
        free(item->runs[i].text);
    }
    free(item->runs);
}


/* Free the narrative and all contents, but not the stylesheet */
void narrative_free(Narrative *n)
{
    int i;
    for ( i=0; i<n->n_items; i++ ) {
        narrative_item_destroy(&n->items[i]);
    }
    free(n->items);
    free(n);
}


void narrative_add_empty_item(Narrative *n)
{
    struct text_run *runs;

    runs = malloc(sizeof(struct text_run));
    if ( runs == NULL ) return;

    runs[0].text = strdup("");
    runs[0].type = TEXT_RUN_NORMAL;
    if ( runs[0].text == NULL ) {
        free(runs);
        return;
    }

    narrative_insert_text(n, n->n_items, runs, 1);
}


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

    n->saved = 1;
    return 0;
}


void narrative_set_unsaved(Narrative *n)
{
    n->saved = 0;
}


int narrative_get_unsaved(Narrative *n)
{
    return !n->saved;
}


int narrative_item_is_text(Narrative *n, int item)
{
    switch ( n->items[item].type ) {
        case NARRATIVE_ITEM_SLIDE : return 0;
        case NARRATIVE_ITEM_EOP : return 0;
        case NARRATIVE_ITEM_TEXT : return 1;
        case NARRATIVE_ITEM_BP : return 1;
        case NARRATIVE_ITEM_PRESTITLE : return 1;
        case NARRATIVE_ITEM_SEGSTART : return 1;
        case NARRATIVE_ITEM_SEGEND : return 0;
    }
    return 1;
}


int narrative_item_is_empty_text(Narrative *n, int item)
{
    struct narrative_item *i = &n->items[item];

    return (narrative_item_is_text(n, item)
         && (i->n_runs == 1)
         && (strlen(i->runs[0].text)==0));
}


const char *narrative_get_language(Narrative *n)
{
    if ( n == NULL ) return NULL;
    return n->language;
}


static void init_item(struct narrative_item *item)
{
    item->runs = NULL;
    item->n_runs = 0;
    item->slide = NULL;
    item->estd_duration = 0.0;
}


static struct narrative_item *add_item(Narrative *n)
{
    struct narrative_item *new_items;
    struct narrative_item *item;
    new_items = realloc(n->items, (n->n_items+1)*sizeof(struct narrative_item));
    if ( new_items == NULL ) return NULL;
    n->items = new_items;
    item = &n->items[n->n_items++];
    item->slide = NULL;
    return item;
}


/* New item will have index 'pos' */
static struct narrative_item *insert_item(Narrative *n, int pos)
{
    int at_end = (pos == n->n_items);
    add_item(n);
    if ( !at_end ) {
        memmove(&n->items[pos+1], &n->items[pos],
                (n->n_items-pos-1)*sizeof(struct narrative_item));
    }
    init_item(&n->items[pos]);
    return &n->items[pos];
}


/* The new text item takes ownership of the array of runs */
static void insert_text_item(Narrative *n, int pos, struct text_run *runs, int n_runs,
                             enum narrative_item_type type)
{
    struct narrative_item *item = insert_item(n, pos);

    if ( item == NULL ) return;

    item->type = type;

    item->runs = runs;
    item->n_runs = n_runs;

    update_timing(item);
}


void narrative_insert_text(Narrative *n, int pos, struct text_run *runs, int n_runs)
{
    insert_text_item(n, pos, runs, n_runs, NARRATIVE_ITEM_TEXT);
}


void narrative_insert_prestitle(Narrative *n, int pos, struct text_run *runs, int n_runs)
{
    insert_text_item(n, pos, runs, n_runs, NARRATIVE_ITEM_PRESTITLE);
}


void narrative_insert_bp(Narrative *n, int pos, struct text_run *runs, int n_runs)
{
    insert_text_item(n, pos, runs, n_runs, NARRATIVE_ITEM_BP);
}


void narrative_insert_segstart(Narrative *n, int pos, struct text_run *runs, int n_runs)
{
    insert_text_item(n, pos, runs, n_runs, NARRATIVE_ITEM_SEGSTART);
}


void narrative_insert_segend(Narrative *n, int pos)
{
    struct narrative_item *item = insert_item(n, pos);
    if ( item == NULL ) return;
    item->type = NARRATIVE_ITEM_SEGEND;
}


void narrative_insert_eop(Narrative *n, int pos)
{
    struct narrative_item *item = insert_item(n, pos);
    if ( item == NULL ) return;
    item->type = NARRATIVE_ITEM_EOP;
}


void narrative_insert_slide(Narrative *n, int pos, Slide *slide)
{
    struct narrative_item *item = insert_item(n, pos);
    if ( item == NULL ) return;
    item->type = NARRATIVE_ITEM_SLIDE;
    item->slide = slide;
    update_timing(item);
}


void narrative_delete_item(Narrative *n, int del)
{
    int i;
    narrative_item_destroy(&n->items[del]);
    for ( i=del; i<n->n_items-1; i++ ) {
        n->items[i] = n->items[i+1];
    }
    n->n_items--;
}


/* o2 = -1 means 'to the end' */
static void delete_text(struct narrative_item *item, size_t o1, ssize_t o2)
{
    int r1, r2;
    size_t roffs1, roffs2;

    r1 = narrative_which_run(item, o1, &roffs1);

    /* This means 'delete to end' */
    if ( o2 == -1 ) {
        int i;
        o2 = 0;
        for ( i=0; i<item->n_runs; i++ ) {
            o2 += strlen(item->runs[i].text);
        }
    }

    r2 = narrative_which_run(item, o2, &roffs2);

    if ( r1 == r2 ) {

        /* Easy case */
        memmove(&item->runs[r1].text[roffs1],
                &item->runs[r2].text[roffs2],
                strlen(item->runs[r1].text)-roffs2+1);

    } else {

        int n_middle;

        /* Truncate the first run */
        item->runs[r1].text[roffs1] = '\0';

        /* Delete any middle runs */
        n_middle = r2 - r1 - 1;
        if ( n_middle > 0 ) {
            memmove(&item->runs[r1+1], &item->runs[r2],
                    (item->n_runs-r2)*sizeof(struct text_run));
            item->n_runs -= n_middle;
            r2 -= n_middle;
        }

        /* Last run */
        memmove(item->runs[r2].text, &item->runs[r2].text[roffs2],
                strlen(&item->runs[r2].text[roffs2])+1);
    }

    update_timing(item);
}


/* Delete from item i1 offset o1 to item i2 offset o2, inclusive */
void narrative_delete_block(Narrative *n, int i1, size_t o1, int i2, size_t o2)
{
    int i;
    int n_del = 0;
    int merge = 1;
    int middle;    /* This is where the "middle deletion" will begin */

    /* Starting item */
    if ( !narrative_item_is_text(n, i1) ) {
        narrative_delete_item(n, i1);
        if ( i1 == i2 ) return;  /* only one slide to delete */
        middle = i1; /* ... which is now the item just after the slide */
        i2--;
        merge = 0;
    } else {
        if ( i1 == i2 ) {
            delete_text(&n->items[i1], o1, o2);
            update_timing(&n->items[i1]);
            return;  /* easy case */
        } else {
            /* Truncate i1 at o1 */
            delete_text(&n->items[i1], o1, -1);
            middle = i1+1;
        }
    }

    /* Middle items */
    for ( i=middle; i<i2; i++ ) {
        /* Deleting the item moves all the subsequent items up, so the
         * index to be deleted doesn't change. */
        narrative_delete_item(n, middle);
        n_del++;
    }
    i2 -= n_del;

    /* Last item */
    if ( !narrative_item_is_text(n, i2) ) {
        narrative_delete_item(n, i2);
        return;
    }

    /* We end with a text item */
    delete_text(&n->items[i2], 0, o2);

    /* If the start and end points are in different paragraphs, and both
     * of them are text (any kind), merge them.  Note that at this point,
     * we know that i1 and i2 are different because otherwise we
     * would've returned earlier ("easy case"). */
    if ( merge ) {

        struct text_run *i1newruns;
        struct narrative_item *item1;
        struct narrative_item *item2;

        assert(i1 != i2);
        item1 = &n->items[i1];
        item2 = &n->items[i2];

        i1newruns = realloc(item1->runs,
                            (item1->n_runs+item2->n_runs)*sizeof(struct text_run));
        if ( i1newruns == NULL ) return;
        item1->runs = i1newruns;

        for ( i=0; i<item2->n_runs; i++ ) {
            item1->runs[i+item1->n_runs].text = strdup(item2->runs[i].text);
            item1->runs[i+item1->n_runs].type = item2->runs[i].type;
        }
        item1->n_runs += item2->n_runs;

        narrative_delete_item(n, i2);
        update_timing(item1);
    }
}


int narrative_which_run(struct narrative_item *item, size_t item_offs, size_t *run_offs)
{
    int run;
    size_t pos = 0;

    for ( run=0; run<item->n_runs; run++ ) {
        size_t npos = pos + strlen(item->runs[run].text);
        if ( npos >= item_offs ) break;
        pos = npos;
    }
    if ( run_offs != NULL ) {
        *run_offs = item_offs - pos;
    }
    return run;
}


void narrative_split_item(Narrative *n, int i1, size_t o1)
{
    struct narrative_item *item1;
    struct narrative_item *item2;

    item2 = insert_item(n, i1+1);
    item1 = &n->items[i1];  /* NB n->items was realloced by insert_item */
    item2->type = NARRATIVE_ITEM_TEXT;

    if ( narrative_item_is_text(n, i1) ) {

        size_t run_offs;
        int run = narrative_which_run(item1, o1, &run_offs);
        int j;

        item2->n_runs = item1->n_runs - run;
        item2->runs = malloc(item2->n_runs*sizeof(struct text_run));
        for ( j=run; j<item1->n_runs; j++ ) {
            item2->runs[j-run] = item1->runs[j];
        }

        /* Now break the run */
        item2->runs[0].text = strdup(item1->runs[run].text+run_offs);
        item1->runs[run].text[run_offs] = '\0';
        item1->n_runs = run + 1;

    } else {

        /* Splitting a non-text run simply means creating a new
         * plain text item after it */
        item2->runs = malloc(sizeof(struct text_run));
        item2->n_runs = 1;
        item2->runs[0].text = strdup("");
        item2->runs[0].type = TEXT_RUN_NORMAL;;

    }

    update_timing(item1);
    update_timing(item2);
}


int narrative_get_num_items(Narrative *n)
{
    return n->n_items;
}


int narrative_get_num_items_to_eop(Narrative *n)
{
    int i;
    for ( i=0; i<n->n_items; i++ ) {
        if ( n->items[i].type == NARRATIVE_ITEM_EOP ) return i;
    }
    return n->n_items;
}


int narrative_get_num_slides(Narrative *n)
{
    int i;
    int ns = 0;
    for ( i=0; i<n->n_items; i++ ) {
        if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) ns++;
    }
    return ns;
}


/* Return the text between item p1/offset o1 and p2/o2 */
char *narrative_range_as_text(Narrative *n, int p1, size_t o1, int p2, size_t o2)
{
    int i;
    char *t;
    int r1, r2;
    size_t r1offs, r2offs;
    size_t len = 0;
    size_t size = 256;

    t = malloc(size);
    if ( t == NULL ) return NULL;
    t[0] = '\0';

    r1 = narrative_which_run(&n->items[p1], o1, &r1offs);
    r2 = narrative_which_run(&n->items[p2], o2, &r2offs);

    for ( i=p1; i<=p2; i++ ) {

        int r;

        /* Skip non-text runs straight away */
        if ( !narrative_item_is_text(n, i) ) continue;

        for ( r=0; r<n->items[i].n_runs; r++ ) {

            size_t run_text_len, len_to_add, start_run_offs;

            /* Is this run within the range? */
            if ( (i==p1) && (r<r1) ) continue;
            if ( (i==p2) && (r>r2) ) continue;

            run_text_len = strlen(n->items[i].runs[r].text);

            if ( (p1==p2) && (r1==r2) && (r==r1) ) {
                start_run_offs = r1offs;
                len_to_add = r2offs - r1offs;
            } else if ( (i==p1) && (r==r1) ) {
                start_run_offs = r1offs;
                len_to_add = run_text_len - r1offs;
            } else if ( (i==p2) && (r==r2) ) {
                start_run_offs = 0;
                len_to_add = r2offs;
            } else {
                start_run_offs = 0;
                len_to_add = run_text_len;
            }

            if ( len_to_add == 0 ) continue;

            /* Ensure space */
            if ( len + len_to_add + 2 > size ) {
                char *nt;
                size += 256 + len_to_add;
                nt = realloc(t, size);
                if ( nt == NULL ) {
                    fprintf(stderr, "Failed to allocate\n");
                    return NULL;
                }
                t = nt;
            }

            memcpy(&t[len], n->items[i].runs[r].text+start_run_offs,
                   len_to_add);
            len += len_to_add;
            t[len] = '\0';
        }

        if ( i < p2 ) {
            t[len++] = '\n';
            t[len] = '\0';
        }

    }

    return t;
}


Slide *narrative_get_slide(Narrative *n, int para)
{
    if ( para >= n->n_items ) return NULL;
    if ( n->items[para].type != NARRATIVE_ITEM_SLIDE ) return NULL;
    return n->items[para].slide;
}


int narrative_get_slide_number_for_slide(Narrative *n, Slide *s)
{
    int i;
    int ns = 0;
    for ( i=0; i<n->n_items; i++ ) {
        if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) {
            if ( n->items[i].slide == s ) return ns;
            ns++;
        }
    }
    return n->n_items;
}


Slide *narrative_get_slide_by_number(Narrative *n, int pos)
{
    int i;
    int ns = 0;
    for ( i=0; i<n->n_items; i++ ) {
        if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) {
            if ( ns == pos ) return n->items[i].slide;
            ns++;
        }
    }
    return NULL;
}


static double timing_from_wordcount(struct narrative_item *item)
{
    int i;
    int words = 0;

    for ( i=0; i<item->n_runs; i++ ) {
        char *text = item->runs[i].text;
        int j;
        size_t len = strlen(text);
        for ( j=0; j<len; j++ ) {
            if ( text[j] == ' ' ) words++;
        }
    }

    return words / 100.0;
}


/* Find the position (in units of narrative items) of time 'minutes' */
double narrative_find_time_pos(Narrative *n, double minutes)
{
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
}


void update_timing(struct narrative_item *item)
{
    switch ( item->type ) {

        case NARRATIVE_ITEM_TEXT :
        case NARRATIVE_ITEM_BP :
        item->estd_duration = timing_from_wordcount(item);
        break;

        case NARRATIVE_ITEM_PRESTITLE :
        case NARRATIVE_ITEM_EOP :
        case NARRATIVE_ITEM_SEGSTART :
        case NARRATIVE_ITEM_SEGEND :
        item->estd_duration = 0.0;
        break;

        case NARRATIVE_ITEM_SLIDE :
        item->estd_duration = 1.0;
        break;
    }
}


static void debug_runs(struct narrative_item *item)
{
    int j;
    for ( j=0; j<item->n_runs; j++ ) {
        printf("Run %i: '%s'\n", j, item->runs[j].text);
    }
}


void narrative_get_first_slide_size(Narrative *n, double *w, double *h)
{
    int i;
    for ( i=0; i<n->n_items; i++ ) {
        if ( n->items[i].type == NARRATIVE_ITEM_SLIDE ) {
            slide_get_logical_size(n->items[i].slide, w, h);
            return;
        }
    }
    printf("No slides found - using standard slide size\n");
    *w = 1024.0;  *h = 768.0;
}


void narrative_debug(Narrative *n)
{
    int i;

    for ( i=0; i<n->n_items; i++ ) {

        struct narrative_item *item = &n->items[i];
        printf("Item %i ", i);
        switch ( item->type ) {

            case NARRATIVE_ITEM_TEXT :
            printf("(text):\n");
            debug_runs(item);
            break;

            case NARRATIVE_ITEM_BP :
            printf("(bp):\n");
            debug_runs(item);
            break;

            case NARRATIVE_ITEM_PRESTITLE :
            printf("(prestitle):\n");
            debug_runs(item);
            break;

            case NARRATIVE_ITEM_EOP :
            printf("(EOP marker)\n");
            break;

            case NARRATIVE_ITEM_SEGSTART :
            printf("(start of segment):\n");
            debug_runs(item);
            break;

            case NARRATIVE_ITEM_SEGEND :
            printf("(end of segment)\n");
            break;

            case NARRATIVE_ITEM_SLIDE :
            printf("Slide\n");
            break;

        }

    }
}


#ifdef HAVE_MD4C

struct md_parse_ctx {
    Narrative *n;
    enum narrative_item_type type;
    int heading;
    int bold;
    int italic;
    int underline;
    int block_open;
};


static void close_block(struct md_parse_ctx *ps)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(ps->n->textbuf, &end);
    gtk_text_buffer_insert(ps->n->textbuf, &end, "\n", -1);
    update_timing(&ps->n->items[ps->n->n_items-1]);
    ps->block_open = 0;
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
            close_block(ps);
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


static enum text_run_type run_type(struct md_parse_ctx *ps)
{
    if ( ps->bold ) return TEXT_RUN_BOLD;
    if ( ps->italic ) return TEXT_RUN_ITALIC;
    if ( ps->underline ) return TEXT_RUN_UNDERLINE;
    return TEXT_RUN_NORMAL;
}


static const char *block_tag_name(struct md_parse_ctx *ps)
{
    switch ( ps->type ) {
        case NARRATIVE_ITEM_TEXT : return "text";
        case NARRATIVE_ITEM_SEGSTART : return "segstart";
        case NARRATIVE_ITEM_PRESTITLE : return "prestitle";
        case NARRATIVE_ITEM_BP : return "bulletpoint";
    }
    return "text";
}


static const char *run_tag_name(struct md_parse_ctx *ps)
{
    if ( ps->bold ) return "bold";
    if ( ps->italic ) return "italic";
    if ( ps->underline ) return "underline";
    return "normal";
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
    struct narrative_item *item;

    if ( !ps->block_open ) {
        item = add_item(ps->n);
        if ( item == NULL ) return 1;
        init_item(item);
        item->type = ps->type;
        item->runs = NULL;
        item->n_runs = 0;
        ps->block_open = 1;
    } else {
        item = &ps->n->items[ps->n->n_items-1];
    }

    if ( ps->type == NARRATIVE_ITEM_SLIDE ) {

        double w, h;

        item->slide = slide_new();

        if ( strncmp(text, "Slide ", 6) != 0 ) return 1;
        char *tx = strndup(text+6, len-6);
        const char *sc = strchr(tx, ';');
        if ( sc == NULL ) return 1;
        if ( strlen(sc) < 3 ) return 1;

        slide_set_ext_filename(item->slide, strdup(sc+2));
        slide_set_ext_number(item->slide, atoi(tx));
        get_ext_slide_size(item->slide, &w, &h);
        slide_set_logical_size(item->slide, w, h);

        GtkTextIter start;
        gtk_text_buffer_get_end_iter(ps->n->textbuf, &start);
        item->anchor = gtk_text_buffer_create_child_anchor(ps->n->textbuf, &start);

        free(tx);
        close_block(ps);

    } else {
        item->runs = realloc(item->runs, (item->n_runs+1)*sizeof(struct text_run));
        item->runs[item->n_runs].text = strndup(text, len);
        item->runs[item->n_runs].type = run_type(ps);
        item->n_runs++;

        GtkTextIter start;
        gtk_text_buffer_get_end_iter(ps->n->textbuf, &start);
        gtk_text_buffer_insert_with_tags_by_name(ps->n->textbuf, &start, text, len,
                                                 run_tag_name(ps),
                                                 block_tag_name(ps), NULL);
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
    pstate.heading = 0;
    pstate.bold = 0;
    pstate.italic = 0;
    pstate.underline = 0;
    pstate.block_open = 0;
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

    if ( n->n_items == 0 ) {
        /* Presentation is empty.  Add a dummy to start things off */
        narrative_add_empty_item(n);
    }

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(n->textbuf, &start, &end);
    gtk_text_buffer_apply_tag_by_name(n->textbuf, "overall", &start, &end);

    return n;
}
