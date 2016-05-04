/*
 * print.c
 *
 * Copyright © 2016 Thomas White <taw@bitwiz.org.uk>
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
	GtkWidget *combo;
	int slides_only;
	SCBlock *slide;
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

	cr = gtk_print_context_get_cairo_context(ctx);
	pc = gtk_print_context_create_pango_context(ctx);
	w = gtk_print_context_get_width(ctx);
	h = gtk_print_context_get_height(ctx);

	stylesheets[0] = ps->p->stylesheet;
	stylesheets[1] = NULL;

	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	top = interp_and_shape(sc_block_child(ps->slide), stylesheets, NULL,
	                       ps->p->is, ISZ_SLIDESHOW,
	                       page+1, cr,
	                       ps->p->slide_width, ps->p->slide_height,
	                       ps->p->lang);

	recursive_wrap(top, pc);

	recursive_draw(top, cr, ps->p->is, ISZ_SLIDESHOW,
	               0.0, ps->p->slide_height);

	ps->slide = next_slide(ps->p, ps->slide);
}


static void print_begin(GtkPrintOperation *op, GtkPrintContext *ctx, void *vp)
{
	struct print_stuff *ps = vp;

	if ( ps->slides_only ) {
		gtk_print_operation_set_n_pages(op, num_slides(ps->p));
		ps->slide = first_slide(ps->p);
	} else {
		/* FIXME */
		printf("Don't know how to print the narrative yet\n");
	}
}


static void print_draw(GtkPrintOperation *op, GtkPrintContext *ctx, gint page,
                       void *vp)
{
	struct print_stuff *ps = vp;
	if ( ps->slides_only ) {
		print_slide_only(op, ctx, ps, page);
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

