/*
 * render_test_sc1.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <assert.h>

#include "../src/storycode.h"
#include "../src/render.h"
#include "../src/stylesheet.h"
#include "../src/frame.h"


static gint mw_destroy(GtkWidget *w, void *p)
{
	exit(0);
}

static gboolean draw_sig(GtkWidget *da, cairo_t *cr, struct frame *fr)
{
	gint w, h;
	double w_used, h_used;

	w = gtk_widget_get_allocated_width(da);
	h = gtk_widget_get_allocated_height(da);

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	cairo_fill(cr);

	//render_frame(fr, cr, w, h, &w_used, &h_used);

	return FALSE;
}


int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *drawingarea;
	struct frame *fr;
	struct style *sty;
	struct style *sty2;

	gtk_init(&argc, &argv);

	fr = sc_unpack("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec a diam lectus. Sed sit amet ipsum mauris. Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit. Donec et mollis dolor. Praesent et diam eget libero egestas mattis sit amet vitae augue. Nam tincidunt congue enim, ut porta lorem lacinia consectetur. \\f{Donec ut libero sed arcu vehicula ultricies a non tortor. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean ut gravida lorem. Ut turpis felis, pulvinar a semper sed, adipiscing id dolor. Pellentesque auctor nisi id magna consequat sagittis. Curabitur dapibus enim sit amet elit pharetra tincidunt feugiat nisl imperdiet. Ut convallis libero in urna ultrices accumsan. Donec sed odio eros.} Donec viverra mi quis quam pulvinar at malesuada arcu rhoncus. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. In rutrum accumsan ultricies. Mauris vitae nisi at sem facilisis semper ac in est.");

	sty = calloc(1, sizeof(struct style));
	sty->lop.pad_l = 10.0;
	sty->lop.pad_r = 10.0;
	sty->lop.pad_t = 10.0;
	sty->lop.pad_b = 10.0;
	sty->lop.margin_l = 0.0;
	sty->lop.margin_r = 0.0;
	sty->lop.margin_t = 0.0;
	sty->lop.margin_b = 0.0;
	sty->name = strdup("Default");

	sty2 = calloc(1, sizeof(struct style));
	sty2->lop.pad_l = 0.0;
	sty2->lop.pad_r = 0.0;
	sty2->lop.pad_t = 0.0;
	sty2->lop.pad_b = 0.0;
	sty2->lop.margin_l = 20.0;
	sty2->lop.margin_r = 20.0;
	sty2->lop.margin_t = 20.0;
	sty2->lop.margin_b = 20.0;
	sty2->name = strdup("Subframe1");

	fr->style = sty;
	fr->children[1]->style = sty2;

	assert(fr->children[0] == fr);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(drawingarea));
	gtk_widget_set_size_request(GTK_WIDGET(drawingarea), 320, 200);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(mw_destroy),
	                 NULL);

	g_signal_connect(G_OBJECT(drawingarea), "draw",
			 G_CALLBACK(draw_sig), fr);

	gtk_widget_show_all(window);
	gtk_main();

	return 0;
}
