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

#include "colloquium.h"
#include "presentation.h"
#include "slide_window.h"
#include "render.h"
#include "frame.h"
#include "slideshow.h"
#include "pr_clock.h"
#include "sc_parse.h"
#include "sc_interp.h"
#include "sc_editor.h"


struct _slidewindow
{
	struct presentation *p;
	GtkWidget           *window;
	GtkToolItem         *bfirst;
	GtkToolItem         *bprev;
	GtkToolItem         *bnext;
	GtkToolItem         *blast;

	/* The slide being displayed.  Note that the SCEditor is using the
	 * child of this block, but we need to keep a record of it for changing
	 * the slide. */
	SCBlock             *scblocks;

	SCEditor            *sceditor;

	struct menu_pl      *style_menu;
	int                  n_style_menu;
};


/* Inelegance to make furniture selection menus work */
struct menu_pl
{
	SlideWindow *sw;
	char *style_name;
	GtkWidget *widget;
};


static void insert_slidetitle_sig(GSimpleAction *action, GVariant *parameter,
                                  gpointer vp)
{
	SlideWindow *sw = vp;
	sc_editor_add_storycode(sw->sceditor, "\\slidetitle{Slide title}");
}


static void paste_sig(GSimpleAction *action, GVariant *parameter,
                      gpointer vp)
{
	SlideWindow *sw = vp;
	sc_editor_paste(sw->sceditor);
}


static void copy_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	SlideWindow *sw = vp;
	sc_editor_copy_selected_frame(sw->sceditor);
}


static void delete_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	SlideWindow *sw = vp;
	sc_editor_delete_selected_frame(sw->sceditor);
}


/* Change the editor's slide to "np" */
void change_edit_slide(SlideWindow *sw, SCBlock *np)
{
	sc_editor_set_slidenum(sw->sceditor, slide_number(sw->p, np));

	sc_editor_set_scblock(sw->sceditor, np);
	sw->scblocks = np;
}


static void change_slide_first(SlideWindow *sw)
{
	change_edit_slide(sw, first_slide(sw->p));
}


static void change_slide_backwards(SlideWindow *sw)
{
	SCBlock *tt;
	tt = prev_slide(sw->p, sw->scblocks);
	if ( tt == NULL ) return;
	change_edit_slide(sw, tt);
}


static void change_slide_forwards(SlideWindow *sw)
{
	SCBlock *tt;
	tt = next_slide(sw->p, sw->scblocks);
	if ( tt == NULL ) return;
	change_edit_slide(sw, tt);
}


static void change_slide_last(SlideWindow *sw)
{
	change_edit_slide(sw, last_slide(sw->p));
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


void slidewindow_redraw(SlideWindow *sw)
{
	sc_editor_redraw(sw->sceditor);
}


static gboolean close_sig(GtkWidget *w, SlideWindow *sw)
{
	sw->p->slidewindow = NULL;
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
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


GActionEntry sw_entries[] = {

	{ "paste", paste_sig, NULL, NULL, NULL },
	{ "copyframe", copy_frame_sig, NULL, NULL, NULL },
	{ "deleteframe", delete_frame_sig, NULL, NULL, NULL },
	{ "first", first_slide_sig, NULL, NULL, NULL },
	{ "prev", prev_slide_sig, NULL, NULL, NULL },
	{ "next", next_slide_sig, NULL, NULL, NULL },
	{ "last", last_slide_sig, NULL, NULL, NULL },
	{ "slidetitle", insert_slidetitle_sig, NULL, NULL, NULL },
};


SlideWindow *slide_window_open(struct presentation *p, SCBlock *scblocks,
                               GApplication *papp)
{
	GtkWidget *window;
	GtkWidget *scroll;
	SlideWindow *sw;
	SCBlock *stylesheets[2];
	SCBlock *ch;
	Colloquium *app = COLLOQUIUM(papp);

	sw = calloc(1, sizeof(SlideWindow));
	if ( sw == NULL ) return NULL;

	window = gtk_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_role(GTK_WINDOW(window), "slide");
	sw->window = window;
	sw->p = p;

	g_action_map_add_action_entries(G_ACTION_MAP(window), sw_entries,
	                                G_N_ELEMENTS(sw_entries), sw);

	g_signal_connect(G_OBJECT(window), "destroy",
	                 G_CALLBACK(close_sig), sw);

	stylesheets[0] = p->stylesheet;
	stylesheets[1] = NULL;

	sw->scblocks = scblocks;
	sw->sceditor = sc_editor_new(scblocks, stylesheets, p->lang,
	                             colloquium_get_imagestore(app));
	sc_editor_set_slidenum(sw->sceditor, slide_number(sw->p, scblocks));
	sc_editor_set_scale(sw->sceditor, 1);

//	scroll = gtk_scrolled_window_new(NULL, NULL);
//	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
//	                               GTK_POLICY_AUTOMATIC,
//	                               GTK_POLICY_AUTOMATIC);
//	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(sw->sceditor));
//	gtk_window_set_focus(GTK_WINDOW(window), GTK_WIDGET(sw->sceditor));
	g_signal_connect(G_OBJECT(sw->sceditor), "key-press-event",
			 G_CALLBACK(key_press_sig), sw);

	sc_editor_set_logical_size(sw->sceditor,
	                           p->slide_width, p->slide_height);

	gtk_window_set_default_size(GTK_WINDOW(window),
	                            p->slide_width, p->slide_height);

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(sw->sceditor));

	/* Default size */
//	gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll),
//	                                          1024);
//	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll),
//	                                           768);
	gtk_window_set_resizable(GTK_WINDOW(sw->window), TRUE);

	gtk_widget_show_all(window);
//	gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll),
//	                                          100);
//	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll),
//	                                           100);

	return sw;
}
