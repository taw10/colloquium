/*
 * print.c
 *
 * Copyright © 2016-2019 Thomas White <taw@bitwiz.org.uk>
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

#include <gtk/gtk.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <libintl.h>
#define _(x) gettext(x)

#include "narrative.h"
#include "narrative_window.h"
#include "slide_render_cairo.h"
#include "narrative_render_cairo.h"


static GtkPrintSettings *print_settings = NULL;

struct print_stuff
{
    Narrative *n;

    /* When printing narrative */
    int nar_line;
    int start_paras[256];
    int slide_number;
};


static void print_begin(GtkPrintOperation *op, GtkPrintContext *ctx, void *vp)
{
    struct print_stuff *ps = vp;
    PangoContext *pc;
    int i, n_pages;
    double h, page_height;
    struct edit_pos sel;

    ps->slide_number = 1;

    pc = gtk_print_context_create_pango_context(ctx);

    sel.para = 0;  sel.pos = 0;  sel.trail = 0;
    narrative_wrap_range(ps->n, narrative_get_stylesheet(ps->n),
                         pango_language_get_default(), pc,
                         gtk_print_context_get_width(ctx),
                         0, narrative_get_num_items(ps->n), sel, sel);

    /* Count pages */
    page_height = gtk_print_context_get_height(ctx);
    h = 0.0;
    n_pages = 1;
    ps->start_paras[0] = 0;
    for ( i=0; i<narrative_get_num_items(ps->n); i++ ) {
        if ( h + narrative_item_get_height(ps->n, i) > page_height ) {
            /* Paragraph does not fit on page */
            ps->start_paras[n_pages] = i;
            n_pages++;
            h = 0.0;
        }
        h += narrative_item_get_height(ps->n, i);
    }
    gtk_print_operation_set_n_pages(op, n_pages);
    g_object_unref(pc);
}


static void print_narrative(GtkPrintOperation *op, GtkPrintContext *ctx,
                            struct print_stuff *ps, gint page)
{
    int i;
    double h, page_height;
    cairo_t *cr;

    page_height = gtk_print_context_get_height(ctx);
    cr = gtk_print_context_get_cairo_context(ctx);

    h = 0.0;
    for ( i=ps->start_paras[page]; i<narrative_get_num_items(ps->n); i++ ) {

        /* Will this paragraph fit? */
        h += narrative_item_get_height(ps->n, i);
        if ( h > page_height ) return;

        cairo_save(cr);
        narrative_render_item_cairo(ps->n, cr, i);
        cairo_restore(cr);

        cairo_translate(cr, 0.0, narrative_item_get_height(ps->n, i));

    }
}


static void print_draw(GtkPrintOperation *op, GtkPrintContext *ctx, gint page,
                       void *vp)
{
    struct print_stuff *ps = vp;
    print_narrative(op, ctx, ps, page);
}


void run_printing(Narrative *n, GtkWidget *parent)
{
    GtkPrintOperation *print;
    GtkPrintOperationResult res;
    struct print_stuff *ps;

    ps = malloc(sizeof(struct print_stuff));
    if ( ps == NULL ) return;
    ps->n = n;
    ps->nar_line = 0;

    print = gtk_print_operation_new();
    if ( print_settings != NULL ) {
        gtk_print_operation_set_print_settings(print, print_settings);
    }

    g_signal_connect(print, "begin_print", G_CALLBACK(print_begin), ps);
    g_signal_connect(print, "draw_page", G_CALLBACK(print_draw), ps);

    res = gtk_print_operation_run(print,
                                  GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                  GTK_WINDOW(parent), NULL);

    if ( res == GTK_PRINT_OPERATION_RESULT_APPLY ) {
        if ( print_settings != NULL ) {
            g_object_unref(print_settings);
        }
        print_settings = g_object_ref(gtk_print_operation_get_print_settings(print));
    }
    g_object_unref(print);
}

