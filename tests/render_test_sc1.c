/*
 * render_test_sc1.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012-2014 Thomas White <taw@bitwiz.org.uk>
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

#include "../src/sc_parse.h"
#include "../src/render.h"
#include "../src/frame.h"


const char *sc = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec a diam lectus. Sed sit amet ipsum mauris. Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit. Donec et mollis dolor. Praesent et diam eget libero egestas mattis sit amet vitae augue. Nam tincidunt congue enim, ut porta lorem lacinia consectetur. \\f[0.5fx0.5f+100+40]{\\bgcol[#ff00ff]Donec ut libero sed arcu vehicula ultricies a non tortor. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean ut gravida lorem. Ut turpis felis, pulvinar a semper sed, adipiscing id dolor. Pellentesque auctor nisi id magna consequat sagittis. Curabitur dapibus enim sit amet elit pharetra tincidunt feugiat nisl imperdiet. Ut convallis libero in urna ultrices accumsan. Donec sed odio eros.} Donec viverra mi quis quam pulvinar at malesuada arcu rhoncus. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. In rutrum accumsan ultricies. Mauris vitae nisi at sem facilisis semper ac in est.";


static gint mw_destroy(GtkWidget *w, void *p)
{
	exit(0);
}


static void unset_all_frames(SCBlock *bl)
{
	while ( bl != NULL ) {
		sc_block_set_frame(bl, NULL);
		if ( sc_block_child(bl) != NULL ) {
			unset_all_frames(sc_block_child(bl));
		}
		if ( sc_block_macro_child(bl) != NULL ) {
			unset_all_frames(sc_block_macro_child(bl));
		}
		bl = sc_block_next(bl);
	}
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, gpointer data)
{
	gint w, h;
	cairo_surface_t *surface;
	SCBlock *scblocks = data;
	struct frame top;

	w = gtk_widget_get_allocated_width(da);
	h = gtk_widget_get_allocated_height(da);

	top.pad_l = 20.0;
	top.pad_r = 20.0;
	top.pad_t = 20.0;
	top.pad_b = 20.0;
	top.w = w;
	top.h = h;
	top.grad = GRAD_NONE;
	top.bgcol[0] = 1.0;
	top.bgcol[1] = 1.0;
	top.bgcol[2] = 0.6;
	top.bgcol[3] = 1.0;

	top.lines = NULL;
	top.n_lines = 0;
	top.children = NULL;
	top.num_children = 0;
	top.max_children = 0;
	top.boxes = NULL;
	unset_all_frames(scblocks);

	surface = render_sc(scblocks, w, h, w, h, &top, NULL, NULL,
	                    ISZ_EDITOR, 1);
	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_surface(cr, surface, 0.0, 0.0);
	cairo_fill(cr);
	cairo_surface_destroy(surface);

	return FALSE;
}


int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *drawingarea;
	SCBlock *scblocks;

	gtk_init(&argc, &argv);

	scblocks = sc_parse(sc);
	if ( scblocks == NULL ) {
		fprintf(stderr, "SC parse failed.\n");
		return 1;
	}
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(drawingarea));
	gtk_widget_set_size_request(GTK_WIDGET(drawingarea), 320, 200);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(mw_destroy),
	                 NULL);

	g_signal_connect(G_OBJECT(drawingarea), "draw",
			 G_CALLBACK(draw_sig), scblocks);

	gtk_widget_show_all(window);
	gtk_main();

	return 0;
}
