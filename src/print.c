/*
 * print.c
 *
 * Copyright © 2016-2018 Thomas White <taw@bitwiz.org.uk>
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

#include "presentation.h"
#include "narrative_window.h"
#include "render.h"


static GtkPrintSettings *print_settings = NULL;

struct print_stuff
{
	struct presentation *p;

	/* Printing config */
	GtkWidget *combo;
	int slides_only;

	/* When printing slides only */
	SCBlock *slide;

	/* When printing narrative */
	int nar_line;
	struct frame *top;
	int start_paras[256];
	int slide_number;

	ImageStore *is;
	const char *storename;
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
	                          "Print the slides only");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cbox), "narrative",
	                          "Print the narrative");
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
	SCBlock *stylesheets[2];
	struct frame *top;
	const double sw = ps->p->slide_width;
	const double sh = ps->p->slide_height;
	double slide_width, slide_height;

	cr = gtk_print_context_get_cairo_context(ctx);
	pc = gtk_print_context_create_pango_context(ctx);
	w = gtk_print_context_get_width(ctx);
	h = gtk_print_context_get_height(ctx);

	stylesheets[0] = ps->p->stylesheet;
	stylesheets[1] = NULL;

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

	top = interp_and_shape(sc_block_child(ps->slide), stylesheets, NULL,
	                       ps->p->is, page+1, pc, sw, sh, ps->p->lang);

	recursive_wrap(top, pc);

	cairo_scale(cr, slide_width/sw, slide_width/sw);

	recursive_draw(top, cr, ps->p->is,
	               0.0, ps->p->slide_height);

	ps->slide = next_slide(ps->p, ps->slide);
}


static int create_thumbnail(SCInterpreter *scin, SCBlock *bl,
                            double *w, double *h, void **bvp, void *vp)
{
	struct print_stuff *ps = vp;
	struct presentation *p = ps->p;
	SCBlock *b;

	*w = 270.0*(p->slide_width / p->slide_height);
	*h = 270.0;
	b = sc_interp_get_macro_real_block(scin);

	*bvp = b;

	return 1;
}


static cairo_surface_t *render_thumbnail(int w, int h, void *bvp, void *vp)
{
	struct print_stuff *ps = vp;
	struct presentation *p = ps->p;
	SCBlock *scblocks = bvp;
	cairo_surface_t *surf;
	SCBlock *stylesheets[2];
	struct frame *top;

	scblocks = sc_block_child(scblocks);
	stylesheets[0] = p->stylesheet;
	stylesheets[1] = NULL;
	surf = render_sc(scblocks, w, h, p->slide_width, p->slide_height, stylesheets, NULL,
	                 p->is, ps->slide_number++, &top, p->lang);
	frame_free(top);

	return surf;
}


static SCBlock *narrative_stylesheet()
{
	return sc_parse("\\stylesheet{"
	                "\\ss[slide]{\\callback[sthumb]}"
	                "}");
}


static void begin_narrative_print(GtkPrintOperation *op, GtkPrintContext *ctx,
                                  struct print_stuff *ps)
{
	SCBlock *stylesheets[3];
	SCCallbackList *cbl;
	PangoContext *pc;
	int i, n_pages;
	double h, page_height;

	cbl = sc_callback_list_new();
	ps->slide_number = 1;
	sc_callback_list_add_callback(cbl, "sthumb", create_thumbnail,
	                              render_thumbnail, NULL, ps);

	ps->is = imagestore_new(ps->storename);

	if ( ps->p->stylesheet != NULL ) {
		stylesheets[0] = ps->p->stylesheet;
		stylesheets[1] = narrative_stylesheet();
		stylesheets[2] = NULL;
	} else {
		stylesheets[0] = narrative_stylesheet();
		stylesheets[1] = NULL;
	}

	pc = gtk_print_context_create_pango_context(ctx);

	ps->top = interp_and_shape(ps->p->scblocks, stylesheets, cbl,
	                           ps->is, 0, pc,
	                           gtk_print_context_get_width(ctx),
	                           gtk_print_context_get_height(ctx),
	                           ps->p->lang);
	recursive_wrap(ps->top, pc);

	/* Count pages */
	page_height = gtk_print_context_get_height(ctx);
	h = 0.0;
	n_pages = 1;
	ps->start_paras[0] = 0;
	for ( i=0; i<ps->top->n_paras; i++ ) {
		if ( h + paragraph_height(ps->top->paras[i]) > page_height ) {
			/* Paragraph does not fit on page */
			ps->start_paras[n_pages] = i;
			n_pages++;
			h = 0.0;
		}
		h += paragraph_height(ps->top->paras[i]);
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
	for ( i=ps->start_paras[page]; i<ps->top->n_paras; i++ ) {

		/* Will this paragraph fit? */
		h += paragraph_height(ps->top->paras[i]);
		if ( h > page_height ) return;

		render_paragraph(cr, ps->top->paras[i], ps->is);
		cairo_translate(cr, 0.0, paragraph_height(ps->top->paras[i]));

	}


}



static void print_begin(GtkPrintOperation *op, GtkPrintContext *ctx, void *vp)
{
	struct print_stuff *ps = vp;

	if ( ps->slides_only ) {
		gtk_print_operation_set_n_pages(op, num_slides(ps->p));
		ps->slide = first_slide(ps->p);
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


void print_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	struct presentation *p = vp;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	struct print_stuff *ps;

	ps = malloc(sizeof(struct print_stuff));
	if ( ps == NULL ) return;
	ps->p = p;
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
	                              GTK_WINDOW(NULL), NULL);

	if ( res == GTK_PRINT_OPERATION_RESULT_APPLY ) {
		if ( print_settings != NULL ) {
			g_object_unref(print_settings);
		}
		print_settings = g_object_ref(
				 gtk_print_operation_get_print_settings(print));
	}
	g_object_unref(print);
}

