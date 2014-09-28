/*
 * narrative_window.c
 *
 * Copyright © 2014 Thomas White <taw@bitwiz.org.uk>
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

#include "presentation.h"
#include "narrative_window.h"


struct _narrative_window
{
	GtkWidget *window;
	GtkWidget *drawingarea;  /* FIXME: Should be an SCEditor */
	GApplication *app;
	struct presentation *p;
};


static void save_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void exportpdf_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
}


GActionEntry nw_entries[] = {

	{ "save", save_sig, NULL, NULL, NULL, },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },

#if 0
		{ "EditAction", NULL, "_Edit", NULL, NULL, NULL },
		{ "SorterAction", NULL, "_Open Slide Sorter...",
			NULL, NULL, G_CALLBACK(open_slidesorter_sig) },
		{ "UndoAction", GTK_STOCK_UNDO, "_Undo",
			NULL, NULL, NULL },
		{ "RedoAction", GTK_STOCK_REDO, "_Redo",
			NULL, NULL, NULL },
		{ "CutAction", GTK_STOCK_CUT, "Cut",
			NULL, NULL, NULL },
		{ "CopyAction", GTK_STOCK_COPY, "Copy",
			NULL, NULL, NULL },
		{ "PasteAction", GTK_STOCK_PASTE, "Paste",
			NULL, NULL, NULL },
		{ "DeleteFrameAction", GTK_STOCK_DELETE, "Delete Frame",
			NULL, NULL, G_CALLBACK(delete_frame_sig) },
//		{ "EditStyleAction", NULL, "Stylesheet...",
//			NULL, NULL, G_CALLBACK(open_stylesheet_sig) },

		{ "InsertAction", NULL, "_Insert", NULL, NULL, NULL },
		{ "NewSlideAction", GTK_STOCK_ADD, "_New Slide",
			NULL, NULL, G_CALLBACK(add_slide_sig) },

		{ "ToolsAction", NULL, "_Tools", NULL, NULL, NULL },
		{ "TSlideshowAction", GTK_STOCK_FULLSCREEN, "_Start Slideshow",
			"F5", NULL, G_CALLBACK(start_slideshow_sig) },
		{ "NotesAction", NULL, "_Open slide notes",
			"F8", NULL, G_CALLBACK(open_notes_sig) },
		{ "ClockAction", NULL, "_Open presentation clock",
			"F9", NULL, G_CALLBACK(open_clock_sig) },
		{ "PrefsAction", GTK_STOCK_PREFERENCES, "_Preferences",
		        NULL, NULL, NULL },

		{ "HelpAction", NULL, "_Help", NULL, NULL, NULL },
		{ "AboutAction", GTK_STOCK_ABOUT, "_About...",
			NULL, NULL,  G_CALLBACK(about_sig) },

		{ "SlideshowAction", GTK_STOCK_FULLSCREEN, "Start Presentation",
			NULL, NULL, G_CALLBACK(start_slideshow_sig) },
		{ "AddSlideAction", GTK_STOCK_ADD, "Add Slide",
			NULL, NULL, G_CALLBACK(add_slide_sig) },
		{ "ButtonFirstSlideAction", GTK_STOCK_GOTO_FIRST, "First Slide",
			NULL, NULL, G_CALLBACK(first_slide_sig) },
		{ "ButtonPrevSlideAction", GTK_STOCK_GO_BACK, "Previous Slide",
			NULL, NULL, G_CALLBACK(prev_slide_sig) },
		{ "ButtonNextSlideAction", GTK_STOCK_GO_FORWARD, "Next Slide",
			NULL, NULL, G_CALLBACK(next_slide_sig) },
		{ "ButtonLastSlideAction", GTK_STOCK_GOTO_LAST, "Last Slide",
			NULL, NULL, G_CALLBACK(last_slide_sig) },
#endif

};


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 NarrativeWindow *nw)
{
	if ( event->type == GDK_2BUTTON_PRESS ) {
		nw->p->slidewindow = slide_window_open(nw->p, nw->app);
	}

	return 0;
}


NarrativeWindow *narrative_window_new(struct presentation *p, GApplication *app)
{
	NarrativeWindow *nw;
	GtkWidget *vbox;
	GtkWidget *scroll;

	if ( p->narrative_window != NULL ) {
		fprintf(stderr, "Narrative window is already open!\n");
		return NULL;
	}

	nw = calloc(1, sizeof(NarrativeWindow));
	if ( nw == NULL ) return NULL;

	nw->app = app;
	nw->p = p;

	nw->window = gtk_application_window_new(GTK_APPLICATION(app));
	p->narrative_window = nw;

	g_action_map_add_action_entries(G_ACTION_MAP(nw->window), nw_entries,
	                                G_N_ELEMENTS(nw_entries), nw->window);

//	update_titlebar(nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw->window), vbox);

	nw->drawingarea = gtk_drawing_area_new();
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
	                                      GTK_WIDGET(nw->drawingarea));

	g_signal_connect(G_OBJECT(nw->drawingarea), "button-press-event",
	                 G_CALLBACK(button_press_sig), nw);
	gtk_widget_set_can_focus(GTK_WIDGET(nw->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(nw->drawingarea),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all(nw->window);

	return nw;
}
