/*
 * print.c
 *
 * Copyright © 2017-2018 Thomas White <taw@bitwiz.org.uk>
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
#include "frame.h"
#include "utils.h"


#define MAX_DEBUG_RUNS (1024)

struct debugwindow
{
	GtkWidget *window;
	GtkWidget *drawingarea;
	struct frame *fr;
	guint timeout;
};


static void plot_hr(cairo_t *cr, double *ypos, double width)
{
	cairo_move_to(cr, 10.0, *ypos+5.5);
	cairo_line_to(cr, width-20.0, *ypos+5.5);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
	*ypos += 10.0;
}


static void plot_text(cairo_t *cr, double *ypos, PangoFontDescription *fontdesc,
                      const char *tmp)
{
	PangoLayout *layout;
	PangoRectangle ext;

	cairo_move_to(cr, 10.0, *ypos);

	layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, tmp, -1);
	pango_layout_set_font_description(layout, fontdesc);

	pango_layout_get_extents(layout, NULL, &ext);
	*ypos += ext.height/PANGO_SCALE;

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
}


static const char *str_type(enum para_type t)
{
	switch ( t ) {

		/* Text paragraph */
		case PARA_TYPE_TEXT : return _("text");

		/* Callback paragraph */
		case PARA_TYPE_CALLBACK : return _("callback");

		/* Unknown paragraph type */
		default : return _("unknown");
	}
}

static void debug_text_para(Paragraph *para, cairo_t *cr, double *ypos,
                            PangoFontDescription *fontdesc)
{
	int i, nrun;
	char tmp[256];

	nrun = para_debug_num_runs(para);
	/* How many text runs */
	snprintf(tmp, 255, _("  %i runs"), nrun);
	plot_text(cr, ypos, fontdesc, tmp);

	for ( i=0; i<nrun; i++ ) {
		SCBlock *scblock;
		if ( para_debug_run_info(para, i, &scblock) ) {
			/* Failed to get debug info for paragraph */
			plot_text(cr, ypos, fontdesc, _("Error"));
		} else {
			snprintf(tmp, 255, "  Run %i: SCBlock %p", i, scblock);
			plot_text(cr, ypos, fontdesc, tmp);
		}
	}

	snprintf(tmp, 255, "Newline at end: %p",
	         para_debug_get_newline_at_end(para));
	plot_text(cr, ypos, fontdesc, tmp);
}


static gboolean dbg_draw_sig(GtkWidget *da, cairo_t *cr, struct debugwindow *dbgw)
{
	int width, height;
	char tmp[256];
	PangoFontDescription *fontdesc;
	int i;
	double ypos = 10.0;

	/* Background */
	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));
	cairo_set_source_rgba(cr, 1.0, 0.8, 0.8, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_fill(cr);

	if ( dbgw->fr == NULL ) return FALSE;

	fontdesc = pango_font_description_new();
	pango_font_description_set_family_static(fontdesc, "Serif");
	pango_font_description_set_style(fontdesc, PANGO_STYLE_ITALIC);
	pango_font_description_set_absolute_size(fontdesc, 15*PANGO_SCALE);

	snprintf(tmp, 255, "Frame %p has %i paragraphs", dbgw->fr, dbgw->fr->n_paras);
	plot_text(cr, &ypos, fontdesc, tmp);

	for ( i=0; i<dbgw->fr->n_paras; i++ ) {

		enum para_type t = para_type(dbgw->fr->paras[i]);
		SCBlock *scblock = para_scblock(dbgw->fr->paras[i]);

		plot_hr(cr, &ypos, width);
		snprintf(tmp, 255, "Paragraph %i: type %s", i, str_type(t));
		plot_text(cr, &ypos, fontdesc, tmp);

		snprintf(tmp, 255, "SCBlock %p", scblock);
		plot_text(cr, &ypos, fontdesc, tmp);

		if ( t == PARA_TYPE_TEXT ) {
			debug_text_para(dbgw->fr->paras[i], cr, &ypos, fontdesc);
		}

		plot_text(cr, &ypos, fontdesc, "");

	}

	pango_font_description_free(fontdesc);

	return FALSE;
}


static gboolean queue_redraw(void *vp)
{
	struct debugwindow *dbgw = vp;
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(dbgw->drawingarea));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(dbgw->drawingarea));
	gtk_widget_queue_draw_area(GTK_WIDGET(dbgw->drawingarea), 0, 0, w, h);
	return TRUE;
}


static gboolean close_sig(GtkWidget *widget, GdkEvent *event, struct debugwindow *dbgw)
{
	g_source_remove(dbgw->timeout);
	free(dbgw);
	return FALSE;
}


void open_debugger(struct frame *fr)
{
	struct debugwindow *dbgw;
	GtkWidget *scroll;

	if ( fr == NULL ) return;

	dbgw = calloc(1, sizeof(struct debugwindow));
	if ( dbgw == NULL ) return;

	dbgw->fr = fr;

	dbgw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(dbgw->window), "debugger");
	gtk_window_set_title(GTK_WINDOW(dbgw->window), "Colloquium debugger");

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(dbgw->window), scroll);

	dbgw->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(scroll), dbgw->drawingarea);
	gtk_widget_set_size_request(dbgw->drawingarea, 100, 8000);

	g_signal_connect(G_OBJECT(dbgw->drawingarea), "draw",
			 G_CALLBACK(dbg_draw_sig), dbgw);

	g_signal_connect(G_OBJECT(dbgw->window), "delete-event",
			 G_CALLBACK(close_sig), dbgw);

	dbgw->timeout = g_timeout_add(1000, queue_redraw, dbgw);

	gtk_widget_show_all(dbgw->window);
}
