/*
 * slide_window.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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
#include <gtk/gtk.h>
#include <assert.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>

#include <narrative.h>
#include <slide.h>
#include <gtkslideview.h>

#include "colloquium.h"
#include "slide_window.h"


struct _slidewindow
{
	GtkWidget           *window;
	Narrative           *n;
	Slide               *slide;
	GtkWidget           *sv;
};


static void insert_slidetitle_sig(GSimpleAction *action, GVariant *parameter,
                                  gpointer vp)
{
	SlideWindow *sw = vp;
	char **text = malloc(sizeof(char *));
	*text = strdup("Slide title");
	slide_add_slidetitle(sw->slide, text, 1);
	gtk_slide_view_set_slide(sw->sv, sw->slide);
}


static void paste_sig(GSimpleAction *action, GVariant *parameter,
                      gpointer vp)
{
	//SlideWindow *sw = vp;
	//sc_editor_paste(sw->sceditor);
}


static void copy_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	//SlideWindow *sw = vp;
	//sc_editor_copy_selected_frame(sw->sceditor);
}


static void delete_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	//SlideWindow *sw = vp;
	//sc_editor_delete_selected_frame(sw->sceditor);
}


/* Change the editor's slide to "np" */
static void change_edit_slide(SlideWindow *sw, Slide *np)
{
	gtk_slide_view_set_slide(sw->sv, np);
	sw->slide = np;
}


static void change_slide_first(SlideWindow *sw)
{
	Slide *s = narrative_get_slide_by_number(sw->n, 0);
	if ( s != NULL ) change_edit_slide(sw, s);
}


static void change_slide_backwards(SlideWindow *sw)
{
	int slide_n = narrative_get_slide_number_for_slide(sw->n, sw->slide);
	if ( slide_n > 0 ) {
		Slide *s = narrative_get_slide_by_number(sw->n, slide_n-1);
		change_edit_slide(sw, s);
	}
}


static void change_slide_forwards(SlideWindow *sw)
{
	int slide_n = narrative_get_slide_number_for_slide(sw->n, sw->slide);
	Slide *s = narrative_get_slide_by_number(sw->n, slide_n+1);
	if ( s != NULL ) change_edit_slide(sw, s);
}


static void change_slide_last(SlideWindow *sw)
{
	int slide_n = narrative_get_num_slides(sw->n);
	Slide *s = narrative_get_slide_by_number(sw->n, slide_n);
	if ( s != NULL ) change_edit_slide(sw, s);
}


static void first_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	SlideWindow *sw = vp;
	change_slide_first(sw);
}


static void prev_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	SlideWindow *sw = vp;
	change_slide_backwards(sw);
}


static void next_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	SlideWindow *sw = vp;
	change_slide_forwards(sw);
}


static void last_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	SlideWindow *sw = vp;
	change_slide_last(sw);
}


static gboolean sw_close_sig(GtkWidget *w, SlideWindow *sw)
{
	//narrative_window_sw_closed(sw->n->narrative_window, sw);
	return FALSE;
}


static gboolean sw_key_press_sig(GtkWidget *da, GdkEventKey *event,
                                 SlideWindow *sw)
{
	switch ( event->keyval ) {

		case GDK_KEY_Page_Up :
		change_slide_backwards(sw);
		break;

		case GDK_KEY_Page_Down :
		change_slide_forwards(sw);
		break;

	}

	return FALSE;
}


static void about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	SlideWindow *sw = vp;
	open_about_dialog(sw->window);
}


GActionEntry sw_entries[] = {

	{ "about", about_sig, NULL, NULL, NULL },
	{ "paste", paste_sig, NULL, NULL, NULL },
	{ "copyframe", copy_frame_sig, NULL, NULL, NULL },
	{ "deleteframe", delete_frame_sig, NULL, NULL, NULL },
	{ "first", first_slide_sig, NULL, NULL, NULL },
	{ "prev", prev_slide_sig, NULL, NULL, NULL },
	{ "next", next_slide_sig, NULL, NULL, NULL },
	{ "last", last_slide_sig, NULL, NULL, NULL },
	{ "slidetitle", insert_slidetitle_sig, NULL, NULL, NULL },
};


extern SlideWindow *slide_window_open(Narrative *n, Slide *slide,
                                      GApplication *papp)
{
	GtkWidget *window;
	SlideWindow *sw;
	double w, h;
	Colloquium *app = COLLOQUIUM(papp);

	sw = calloc(1, sizeof(SlideWindow));
	if ( sw == NULL ) return NULL;

	window = gtk_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_role(GTK_WINDOW(window), "slide");
	sw->window = window;
	sw->n = n;
	sw->slide = slide;

	g_action_map_add_action_entries(G_ACTION_MAP(window), sw_entries,
	                                G_N_ELEMENTS(sw_entries), sw);

	g_signal_connect(G_OBJECT(window), "destroy",
	                 G_CALLBACK(sw_close_sig), sw);

	sw->sv = gtk_slide_view_new(n, slide);

	g_signal_connect(G_OBJECT(sw->sv), "key-press-event",
			 G_CALLBACK(sw_key_press_sig), sw);

	slide_get_logical_size(slide, narrative_get_stylesheet(n), &w, &h);
	gtk_window_set_default_size(GTK_WINDOW(window), w, h);

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(sw->sv));

	gtk_window_set_resizable(GTK_WINDOW(sw->window), TRUE);

	gtk_widget_show_all(window);

	return sw;
}
