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

	/* Printing config */
	GtkWidget *combo;
	int slides_only;

	/* When printing slides only */
	Slide *slide;

	/* When printing narrative */
	int nar_line;
	int start_paras[256];
	int slide_number;
};


static void print_widget_apply(GtkPrintOperation *op, GtkWidget *widget,
                               void *vp)
{
	const char *id;
	struct print_stuff *ps = vp;

	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(ps->combo));
	if ( strcmp(id, "slides") == 0 ) {
		ps->slides_only = 1;
	} else {
		ps->slides_only = 0;
	}
}


static GObject *print_widget(GtkPrintOperation *op, void *vp)
{
	GtkWidget *vbox;
	GtkWidget *cbox;
	struct print_stuff *ps = vp;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	/* What do you want to print? */
	cbox = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cbox), "slides",
	                          _("Print the slides only"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cbox), "narrative",
	                          _("Print the narrative"));
	gtk_box_pack_start(GTK_BOX(vbox), cbox, FALSE, FALSE, 10);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cbox), 1);
	ps->combo = cbox;

	gtk_widget_show_all(vbox);
	return G_OBJECT(vbox);

}


static void print_slide_only(GtkPrintOperation *op, GtkPrintContext *ctx,
                             struct print_stuff *ps, int page)
{
	cairo_t *cr;
	PangoContext *pc;
	double w, h;
	double sw, sh;
	double slide_width, slide_height;
	struct slide_pos sel;
	int slidenum;

	cr = gtk_print_context_get_cairo_context(ctx);
	pc = gtk_print_context_create_pango_context(ctx);
	w = gtk_print_context_get_width(ctx);
	h = gtk_print_context_get_height(ctx);

	slide_get_logical_size(ps->slide, narrative_get_stylesheet(ps->n),
	                       &sw, &sh);

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	if ( sw/sh > w/h ) {
		/* Slide is too wide.  Letterboxing top/bottom */
		slide_width = w;
		slide_height = w * sh/sw;
	} else {
		/* Letterboxing at sides */
		slide_width = h * sw/sh;
		slide_height = h;
	}

	printf("%f x %f ---> %f x %f\n", w, h, slide_width, slide_height);

	cairo_scale(cr, slide_width/sw, slide_width/sw);

	sel.para = 0;  sel.pos = 0;  sel.trail = 0;
	slidenum = narrative_get_slide_number_for_slide(ps->n,  ps->slide);
	slide_render_cairo(ps->slide, cr, narrative_get_imagestore(ps->n),
	                   narrative_get_stylesheet(ps->n), slidenum,
	                   pango_language_get_default(), pc,
	                   NULL, sel, sel);

	ps->slide = narrative_get_slide_by_number(ps->n, slidenum+1);
}


static void begin_narrative_print(GtkPrintOperation *op, GtkPrintContext *ctx,
                                  struct print_stuff *ps)
{
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
	                     narrative_get_imagestore(ps->n),
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


static void print_begin(GtkPrintOperation *op, GtkPrintContext *ctx, void *vp)
{
	struct print_stuff *ps = vp;

	if ( ps->slides_only ) {
		gtk_print_operation_set_n_pages(op, narrative_get_num_slides(ps->n));
		ps->slide = narrative_get_slide_by_number(ps->n, 0);
	} else {
		begin_narrative_print(op, ctx, ps);
	}
}


static void print_draw(GtkPrintOperation *op, GtkPrintContext *ctx, gint page,
                       void *vp)
{
	struct print_stuff *ps = vp;
	if ( ps->slides_only ) {
		print_slide_only(op, ctx, ps, page);
	} else {
		print_narrative(op, ctx, ps, page);
	}
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

	g_signal_connect(print, "create-custom-widget",
			 G_CALLBACK(print_widget), ps);
	g_signal_connect(print, "custom-widget-apply",
			 G_CALLBACK(print_widget_apply), ps);
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

