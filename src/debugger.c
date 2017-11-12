/*
 * print.c
 *
 * Copyright © 2017 Thomas White <taw@bitwiz.org.uk>
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


#define MAX_DEBUG_RUNS (1024)

struct run_debug
{
	enum para_type para_type;

	int np;
	size_t len;
	void *scblock; /* Don't you dare try to dereference this */
	size_t offs;
	size_t para_offs;
};


struct debugwindow
{
	GtkWidget *window;
	GtkWidget *drawingarea;
	struct frame *fr;
	guint timeout;

	int n_changed;
	int changesig;
	struct run_debug *runs;
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
		case PARA_TYPE_TEXT : return "text";
		case PARA_TYPE_CALLBACK : return "callback";
		default : return "unknown";
	}
}

static void debug_text_para(Paragraph *para, cairo_t *cr, double *ypos,
                            PangoFontDescription *fontdesc,
                            struct run_debug *rd, int *dpos, int *changesig)
{
	int i, nrun;
	char tmp[256];

	nrun = para_debug_num_runs(para);
	snprintf(tmp, 255, "  %i runs", nrun);
	plot_text(cr, ypos, fontdesc, tmp);

	for ( i=0; i<nrun; i++ ) {
		size_t scblock_offs, para_offs, len;
		SCBlock *scblock;
		if ( para_debug_run_info(para, i, &len, &scblock, &scblock_offs, &para_offs) ) {
			plot_text(cr, ypos, fontdesc, "Error");
		} else {

			snprintf(tmp, 255, "  Run %i: len %li, SCBlock %p offs %li, para offs %li",
			         i, len, scblock, scblock_offs, para_offs);
			plot_text(cr, ypos, fontdesc, tmp);
			if ( len != rd[*dpos].len ) {
				snprintf(tmp, 255, "   (len was %li)", rd[*dpos].len);
				plot_text(cr, ypos, fontdesc, tmp);
				(*changesig) += i*(*dpos)*len*(*ypos);
			}
			if ( para_offs != rd[*dpos].para_offs ) {
				snprintf(tmp, 255, "   (para offs was %li)", rd[*dpos].para_offs);
				plot_text(cr, ypos, fontdesc, tmp);
				(*changesig) += i*(*dpos)*para_offs*(*ypos);
			}
			if ( scblock_offs != rd[*dpos].offs ) {
				snprintf(tmp, 255, "   (offs was %li)", rd[*dpos].offs);
				plot_text(cr, ypos, fontdesc, tmp);
				(*changesig) += i*(*dpos)*scblock_offs*(*ypos);
			}
			(*dpos)++;

		}
	}

	snprintf(tmp, 255, "Newline at end: %p\n", get_newline_at_end(para));
	plot_text(cr, ypos, fontdesc, tmp);
}


static void record_runs(struct debugwindow *dbgw)
{
	int i;
	int n = 0;

	for ( i=0; i<dbgw->fr->n_paras; i++ ) {

		int j, nrun;
		Paragraph *para = dbgw->fr->paras[i];

		dbgw->runs[n].para_type = para_type(para);

		if ( para_type(para) != PARA_TYPE_TEXT ) {
			n++;
			continue;
		}

		nrun = para_debug_num_runs(para);

		for ( j=0; j<nrun; j++ ) {

			size_t scblock_offs, para_offs, len;
			SCBlock *scblock;

			if ( para_debug_run_info(para, j, &len, &scblock,
			                         &scblock_offs, &para_offs) )
			{
				continue;
			}

			dbgw->runs[n].np = i;
			dbgw->runs[n].len = len;
			dbgw->runs[n].scblock = scblock;
			dbgw->runs[n].offs = scblock_offs;
			dbgw->runs[n].para_offs = para_offs;
			n++;

			if ( n == MAX_DEBUG_RUNS ) {
				printf("Too many runs to debug\n");
				return;
			}

		}

	}

	dbgw->changesig = 0;
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, struct debugwindow *dbgw)
{
	int width, height;
	char tmp[256];
	PangoFontDescription *fontdesc;
	int i;
	double ypos = 10.0;
	int dpos = 0;
	int changesig = 0;
	int npr = 10;  /* Not zero */

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

		/* Jump the "old values" pointer forward to the next paragraph start */
		while ( dbgw->runs[dpos].np == npr ) dpos++;
		npr = dbgw->runs[dpos].np;

		plot_hr(cr, &ypos, width);
		snprintf(tmp, 255, "Paragraph %i: type %s", i, str_type(t));
		plot_text(cr, &ypos, fontdesc, tmp);

		if ( t == PARA_TYPE_TEXT ) {
			debug_text_para(dbgw->fr->paras[i], cr, &ypos, fontdesc,
			                dbgw->runs, &dpos, &changesig);
		} else {
			dpos++;
		}

	}

	if ( changesig == dbgw->changesig ) {
		dbgw->n_changed++;
		if ( dbgw->n_changed >= 5 ) {
			record_runs(dbgw);
			dbgw->n_changed = 0;
		}
	} else {
		dbgw->changesig = changesig;
		dbgw->n_changed = 0;
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
	dbgw = calloc(1, sizeof(struct debugwindow));
	if ( dbgw == NULL ) return;

	dbgw->fr = fr;
	dbgw->runs = malloc(MAX_DEBUG_RUNS * sizeof(struct run_debug));
	if ( dbgw->runs == NULL ) return;

	dbgw->n_changed = 0;
	record_runs(dbgw);

	dbgw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(dbgw->window), "debugger");
	gtk_window_set_title(GTK_WINDOW(dbgw->window), "Colloquium debugger");

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(dbgw->window), scroll);

	dbgw->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(scroll), dbgw->drawingarea);
	gtk_widget_set_size_request(dbgw->drawingarea, 100, 8000);

	g_signal_connect(G_OBJECT(dbgw->drawingarea), "draw",
			 G_CALLBACK(draw_sig), dbgw);

	g_signal_connect(G_OBJECT(dbgw->window), "delete-event",
			 G_CALLBACK(close_sig), dbgw);

	dbgw->timeout = g_timeout_add(1000, queue_redraw, dbgw);

	gtk_widget_show_all(dbgw->window);
}
