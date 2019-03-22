/*
 * testcard.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libintl.h>
#define _(x) gettext(x)

#include <presentation.h>


struct testcard
{
	GtkWidget *window;
	char geom[256];
	double slide_width;
	double slide_height;
	GtkWidget *drawingarea;
};

static gint tc_destroy_sig(GtkWidget *widget, struct testcard *tc)
{
	free(tc);
	return FALSE;
}


static void arrow_left(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, size, size);
	cairo_rel_line_to(cr, 0.0, -2*size);
	cairo_rel_line_to(cr, -size, size);
}


static void arrow_right(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, -size, size);
	cairo_rel_line_to(cr, 0.0, -2*size);
	cairo_rel_line_to(cr, size, size);
}


static void arrow_down(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, -size, -size);
	cairo_rel_line_to(cr, 2*size, 0.0);
	cairo_rel_line_to(cr, -size, size);
}


static void arrow_up(cairo_t *cr, double size)
{
	cairo_rel_line_to(cr, -size, size);
	cairo_rel_line_to(cr, 2*size, 0.0);
	cairo_rel_line_to(cr, -size, -size);
}


static void colour_box(cairo_t *cr, double x, double y,
                       double r, double g, double b, const char *label)
{
	const double w = 50.0;
	const double h = 50.0;
	cairo_text_extents_t size;

	cairo_rectangle(cr, x+0.5, y+0.5, w, h);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	cairo_set_font_size(cr, 24.0);
	cairo_text_extents(cr, label, &size);
	cairo_move_to(cr, x+(w/2.0)-(size.width/2.0), y-10.0);
	cairo_show_text(cr, label);

}


static gboolean tc_draw_sig(GtkWidget *da, cairo_t *cr, struct testcard *tc)
{
	double xoff, yoff;
	double width, height;
	double w, h;
	PangoLayout *pl;
	PangoFontDescription *desc;
	char tmp[1024];
	int plw, plh;
	double xp, yp;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_fill(cr);

	if ( tc->slide_width/tc->slide_height < width/height ) {
		w = (tc->slide_width / tc->slide_height) * height;
		h = height;
	} else {
		w = width;
		h = (tc->slide_height / tc->slide_width) * width;
	}

	/* Get the overall size */
	xoff = (width - w)/2.0;
	yoff = (height - h)/2.0;

	/* Background of slide */
	cairo_rectangle(cr, xoff, yoff, w, h);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_rectangle(cr, xoff+10.5, yoff+10.5, w-20.0, h-20.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	/* Arrows showing edges of screen */
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_move_to(cr, 0.0, height/2);
	arrow_left(cr, 100.0);
	cairo_fill(cr);
	cairo_move_to(cr, width, height/2);
	arrow_right(cr, 100.0);
	cairo_fill(cr);
	cairo_move_to(cr, width/2, height);
	arrow_down(cr, 100.0);
	cairo_fill(cr);
	cairo_move_to(cr, width/2, 0.0);
	arrow_up(cr, 100.0);
	cairo_fill(cr);

	/* Arrows showing edges of slide */
	cairo_translate(cr, xoff, yoff);
	cairo_set_source_rgb(cr, 0.5, 0.0, 0.0);
	cairo_move_to(cr, 0.0, 100+h/2);
	arrow_left(cr, 80.0);
	cairo_fill(cr);
	cairo_move_to(cr, w, 100+h/2);
	arrow_right(cr, 80.0);
	cairo_fill(cr);
	cairo_move_to(cr, 100+w/2, h);
	arrow_down(cr, 80.0);
	cairo_fill(cr);
	cairo_move_to(cr, 100+w/2, 0.0);
	arrow_up(cr, 80.0);
	cairo_fill(cr);

	cairo_rectangle(cr, 10.5, 10.5, w-20.0, h-20.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	/* Stuff in the middle */
	yp = (h-400)/2.0;
	cairo_save(cr);
	cairo_translate(cr, 0.0, yp);

	snprintf(tmp, 1024, _("Test Card\nColloquium version %s\n"
	                    "Screen resolution %.0f × %.0f\n"
	                    "Slide resolution %.0f × %.0f"),
	                    PACKAGE_VERSION, width, height,
	                    w, h);

	pl = pango_cairo_create_layout(cr);
	desc = pango_font_description_from_string("Sans 24");
	pango_layout_set_font_description(pl, desc);
	pango_layout_set_text(pl, tmp, -1);

	pango_layout_get_size(pl, &plw, &plh);
	plw = pango_units_to_double(plw);
	plh = pango_units_to_double(plh);
	cairo_move_to(cr, (w-plw)/2, 0.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	pango_cairo_show_layout(cr, pl);

	/* Colour boxes */
	xp = (w-450)/2.0;
	colour_box(cr, xp+0,   200, 1.0, 0.0, 0.0, _("Red"));
	colour_box(cr, xp+80,  200, 0.0, 1.0, 0.0, _("Green"));
	colour_box(cr, xp+160, 200, 0.0, 0.0, 1.0, _("Blue"));
	colour_box(cr, xp+240, 200, 1.0, 1.0, 0.0, _("Yellow"));
	colour_box(cr, xp+320, 200, 1.0, 0.0, 1.0, _("Pink"));
	colour_box(cr, xp+400, 200, 0.0, 1.0, 1.0, _("Cyan"));

	/* Shades of grey */
	double i;
	for ( i=0; i<=1.0; i+=0.2 ) {
		char label[32];
		snprintf(label, 31, "%.0f%%", i*100.0);
		colour_box(cr, xp+(i*5*80), 300, i, i, i, label);
	}
	cairo_restore(cr);

	return FALSE;
}


static gboolean tc_key_press_sig(GtkWidget *da, GdkEventKey *event,
                                 struct testcard *tc)
{
	if ( !event->is_modifier ) gtk_widget_destroy(tc->window);
	return FALSE;
}


void show_testcard(Presentation *p)
{
	GdkDisplay *display;
	int n_monitors;
	struct testcard *tc;

	tc = calloc(1, sizeof(struct testcard));
	if ( tc == NULL ) return;

	tc->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	stylesheet_get_slide_default_size(presentation_get_stylesheet(p),
	                                  &tc->slide_width, &tc->slide_height);

	tc->drawingarea = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(tc->window), tc->drawingarea);

	gtk_widget_set_can_focus(GTK_WIDGET(tc->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(tc->drawingarea), GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(tc->drawingarea), "key-press-event",
			 G_CALLBACK(tc_key_press_sig), tc);
	g_signal_connect(G_OBJECT(tc->window), "destroy",
	                 G_CALLBACK(tc_destroy_sig), tc);
	g_signal_connect(G_OBJECT(tc->drawingarea), "draw",
			 G_CALLBACK(tc_draw_sig), tc);

	gtk_widget_grab_focus(GTK_WIDGET(tc->drawingarea));

	display = gdk_display_get_default();
	n_monitors = gdk_display_get_n_monitors(display);

	GdkMonitor *mon_ss;
	if ( n_monitors == 1 ) {
		mon_ss = gdk_display_get_primary_monitor(display);
		printf(_("Single monitor mode\n"));
	} else {
		mon_ss = gdk_display_get_monitor(display, 1);
		printf(_("Dual monitor mode\n"));
	}

	/* Workaround because gtk_window_fullscreen_on_monitor doesn't work */
	GdkRectangle rect;
	gdk_monitor_get_geometry(mon_ss, &rect);
	gtk_window_move(GTK_WINDOW(tc->window), rect.x, rect.y);
	gtk_window_fullscreen(GTK_WINDOW(tc->window));

	gtk_widget_show_all(GTK_WIDGET(tc->window));

}

