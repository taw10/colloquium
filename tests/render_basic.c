/*
 * tests/render_basic.c
 *
 * Copyright © 2012-2018 Thomas White <taw@bitwiz.org.uk>
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

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <assert.h>

#include "../src/sc_parse.h"
#include "../src/render.h"
#include "../src/frame.h"


const char *sc = "\\presentation{Lorem ipsum dolor sit amet, consect-etur adipiscing elit.\nDonec a diam lectus. Sed sit amet ipsum mauris. Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit. Donec et mollis dolor. Praesent et diam eget libero egestas mattis sit amet vitae augue. \\font[Edrip 32]{Nam tincidunt congue enim, ut porta lorem \\font[Edrip Bold 32]{lacinia} consectetur.} Donec ut libero sed arcu vehicula ultricies a non tortor. Lorem ipsum dolor sit amet, consectetur adipiscing elit. \\bold{Aenean} ut gravida lorem. Ut turpis felis, pulvinar a semper sed, adipiscing id dolor. Pellentesque auctor nisi id magna consequat sagittis. Curabitur dapibus enim sit amet elit pharetra tincidunt feugiat nisl imperdiet. Ut convallis libero in urna ultrices accumsan. Donec sed odio eros. Donec viverra mi quis quam pulvinar at malesuada arcu rhoncus. \\font[Serif Bold 17]{Cum sociis natoque penatibus et magnis dis parturient} montes, nascetur ridiculus mus. In rutrum accumsan ultricies. Mauris vitae nisi at sem facilisis semper ac in est.}";

static gint mw_destroy(GtkWidget *w, void *p)
{
	gtk_main_quit();
	exit(0);
}

static gboolean draw_sig(GtkWidget *da, cairo_t *cr, gpointer data)
{
	gint w, h;
	cairo_surface_t *surface;
	SCBlock *scblocks = data;
	struct frame *top;
	PangoLanguage *lang = pango_language_from_string("en_GB");

	w = gtk_widget_get_allocated_width(da);
	h = gtk_widget_get_allocated_height(da);

	surface = render_sc(scblocks, w, h, w, h, NULL, NULL, NULL,
	                    1, &top, lang);
	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_surface(cr, surface, 0.0, 0.0);
	cairo_fill(cr);
	cairo_surface_destroy(surface);
	frame_free(top);

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
	show_sc_blocks(scblocks);

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
