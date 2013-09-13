/*
 * render_test.c
 *
 * Rendering test
 *
 * Copyright © 2012-2013 Thomas White <taw@bitwiz.org.uk>
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

#include "../src/storycode.h"
#include "../src/render.h"
#include "../src/stylesheet.h"
#include "../src/frame.h"
#include "../src/presentation.h"


static gint mw_destroy(GtkWidget *w, void *p)
{
	gtk_main_quit();
	return 0;
}

static gboolean draw_sig(GtkWidget *da, cairo_t *cr, gpointer data)
{
	gint w, h;
	struct slide *s = data;

	w = gtk_widget_get_allocated_width(da);
	h = gtk_widget_get_allocated_height(da);

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	cairo_fill(cr);

	if ( s->rendered_edit != NULL ) cairo_surface_destroy(s->rendered_edit);
	s->rendered_edit = render_slide(s, w, w, h, NULL, ISZ_EDITOR);
	cairo_rectangle(cr, 0.0, 0.0, w, h);
	cairo_set_source_surface(cr, s->rendered_edit, 0.0, 0.0);
	cairo_fill(cr);

	return FALSE;
}


int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *drawingarea;
	struct frame *fr;
	struct frame *fr2;
	struct style *sty1;
	struct style *sty2;
	struct slide s;
	struct presentation p;

	gtk_init(&argc, &argv);

	sty1 = calloc(1, sizeof(struct style));
	sty1->lop.pad_l = 0.0;
	sty1->lop.pad_r = 0.0;
	sty1->lop.pad_t = 0.0;
	sty1->lop.pad_b = 0.0;
	sty1->lop.margin_l = 0.0;
	sty1->lop.margin_r = 0.0;
	sty1->lop.margin_t = 0.0;
	sty1->lop.margin_b = 0.0;
	sty1->lop.x = 0.0;
	sty1->lop.y = 0.0;
	sty1->lop.w = 1.0;
	sty1->lop.w_units = UNITS_FRAC;
	sty1->lop.h = 1.0;
	sty1->lop.h_units = UNITS_FRAC;
	sty1->name = strdup("Default");

	sty2 = calloc(1, sizeof(struct style));
	sty2->lop.pad_l = 10.0;
	sty2->lop.pad_r = 10.0;
	sty2->lop.pad_t = 10.0;
	sty2->lop.pad_b = 10.0;
	sty2->lop.margin_l = 5.0;
	sty2->lop.margin_r = 5.0;
	sty2->lop.margin_t = 5.0;
	sty2->lop.margin_b = 5.0;
	sty2->lop.x = 0.0;
	sty2->lop.y = 0.0;
	sty2->lop.w = 1.0;
	sty2->lop.w_units = UNITS_FRAC;
	sty2->lop.h = 1.0;
	sty2->lop.h_units = UNITS_FRAC;
	sty2->name = strdup("Text frame");

	fr2 = calloc(1, sizeof(struct frame));
	if ( fr2 == NULL ) return 1;
	fr2->sc = "\\font[Sorts Mill Goudy 32]Lorem ipsum dolor sit amet, consect-etur adipiscing elit.\n\\font[Serif 17]Donec a diam lectus. Sed sit amet ipsum mauris. Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit. Donec et mollis dolor. Praesent et diam eget libero egestas mattis sit amet vitae augue. \\font[Edrip 32]{Nam tincidunt congue enim, ut porta lorem \\font[Edrip Bold 32]{lacinia} consectetur.} Donec ut libero sed arcu vehicula ultricies a non tortor. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean ut gravida lorem. Ut turpis felis, pulvinar a semper sed, adipiscing id dolor. Pellentesque auctor nisi id magna consequat sagittis. Curabitur dapibus enim sit amet elit pharetra tincidunt feugiat nisl imperdiet. Ut convallis libero in urna ultrices accumsan. Donec sed odio eros. Donec viverra mi quis quam pulvinar at malesuada arcu rhoncus. \\font[Serif Bold 17]{Cum sociis natoque penatibus et magnis dis parturient} montes, nascetur ridiculus mus. In rutrum accumsan ultricies. Mauris vitae nisi at sem facilisis semper ac in est.";
	//fr2->sc = "Lorem ipsum AT";
	fr2->children = NULL;
	fr2->num_children = 0;
	fr2->style = sty2;
	fr2->lop_from_style = 1;

	fr = calloc(1, sizeof(struct frame));
	if ( fr == NULL ) return 1;
	fr->sc = "";
	fr->children = calloc(2, sizeof(struct frame *));
	if ( fr->children == NULL ) return 1;
	fr->children[0] = fr2;
	fr->style = sty1;
	fr->lop_from_style = 1;
	fr->num_children = 1;

	s.top = fr;
	s.rendered_edit = NULL;
	s.rendered_proj = NULL;
	s.rendered_thumb = NULL;
	s.constants = NULL;
	s.parent = &p;

	p.constants = NULL;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(drawingarea));
	gtk_widget_set_size_request(GTK_WIDGET(drawingarea), 320, 200);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(mw_destroy),
	                 NULL);

	g_signal_connect(G_OBJECT(drawingarea), "draw",
			 G_CALLBACK(draw_sig), &s);

	gtk_widget_show_all(window);
	gtk_main();

	free_render_buffers(&s);

	return 0;
}
