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
#include <string.h>
#include <stdlib.h>

#include "presentation.h"
#include "narrative_window.h"
#include "sc_editor.h"


struct _narrative_window
{
	GtkWidget *window;
	GtkToolItem         *bfirst;
	GtkToolItem         *bprev;
	GtkToolItem         *bnext;
	GtkToolItem         *blast;
	SCEditor *sceditor;
	GApplication *app;
	struct presentation *p;
};


static gint saveas_response_sig(GtkWidget *d, gint response,
                                NarrativeWindow *nw)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		if ( save_presentation(nw->p, filename) ) {
			//show_error(sw, "Failed to save presentation");
		}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static void saveas_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GtkWidget *d;
	NarrativeWindow *nw = vp;

	d = gtk_file_chooser_dialog_new("Save Presentation",
	                                GTK_WINDOW(nw->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                "_Open", GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(saveas_response_sig), nw);

	gtk_widget_show_all(d);
}


static void save_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;

	if ( nw->p->filename == NULL ) {
		return saveas_sig(NULL, NULL, nw);
	}

	save_presentation(nw->p, nw->p->filename);
}


static void exportpdf_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
}


static void open_slidesorter_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void delete_frame_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void add_slide_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void start_slideshow_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void open_notes_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void open_clock_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


GActionEntry nw_entries[] = {

	{ "save", save_sig, NULL, NULL, NULL },
	{ "saveas", saveas_sig, NULL, NULL, NULL },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },
	{ "sorter", open_slidesorter_sig, NULL, NULL, NULL },
	{ "deleteframe", delete_frame_sig, NULL, NULL, NULL },
	{ "slide", add_slide_sig, NULL, NULL, NULL },
	{ "startslideshow", start_slideshow_sig, NULL, NULL, NULL },
	{ "notes", open_notes_sig, NULL, NULL, NULL },
	{ "clock", open_clock_sig, NULL, NULL, NULL },
};


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 NarrativeWindow *nw)
{
	if ( event->type == GDK_2BUTTON_PRESS ) {
		nw->p->slidewindow = slide_window_open(nw->p, nw->app);
	}

	return 0;
}


static void nw_update_titlebar(NarrativeWindow *nw)
{
	get_titlebar_string(nw->p);

	if ( nw->p->slidewindow != NULL ) {

		char *title;

		title = malloc(strlen(nw->p->titlebar)+14);
		sprintf(title, "%s - Colloquium", nw->p->titlebar);
		gtk_window_set_title(GTK_WINDOW(nw->window), title);
		free(title);

       }

}


static void update_toolbar(NarrativeWindow *nw)
{
}


static SCBlock *narrative_stylesheet()
{
	return sc_parse("\\stylesheet{\\ss[slide]{\n\\callback[sthumb]\n}}");
}


static cairo_surface_t *render_thumbnail(SCInterpreter *scin, SCBlock *bl,
                                         void *vp)
{
	struct presentation *p = vp;
	cairo_surface_t *surf;
	cairo_t *cr;

	surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 256, 256);
	cr = cairo_create(surf);
	cairo_set_source_rgb(cr, 0.0, 0.5, 0.0);
	cairo_paint(cr);
	cairo_destroy(cr);

	return surf;
}


NarrativeWindow *narrative_window_new(struct presentation *p, GApplication *app)
{
	NarrativeWindow *nw;
	GtkWidget *vbox;
	GtkWidget *scroll;
	GtkWidget *toolbar;
	GtkToolItem *button;
	SCBlock *stylesheets[3];
	SCCallbackList *cbl;

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
	                                G_N_ELEMENTS(nw_entries), nw);

	nw_update_titlebar(nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw->window), vbox);

	stylesheets[0] = p->stylesheet;
	stylesheets[1] = narrative_stylesheet();
	stylesheets[2] = NULL;
	nw->sceditor = sc_editor_new(nw->p->scblocks, stylesheets);
	cbl = sc_callback_list_new();
	sc_callback_list_add_callback(cbl, "sthumb", render_thumbnail, NULL);
	sc_editor_set_callbacks(nw->sceditor, cbl);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(toolbar), FALSE, FALSE, 0);

	/* Fullscreen */
	button = gtk_tool_button_new(gtk_image_new_from_icon_name("view-fullscreen", GTK_ICON_SIZE_LARGE_TOOLBAR), "Start slideshow");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.startslideshow");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	/* Add slide */
	button = gtk_tool_button_new_from_stock(GTK_STOCK_ADD);
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	/* Change slide */
	nw->bfirst = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_FIRST);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bfirst));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bfirst),
	                               "win.first");
	nw->bprev = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bprev));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bprev),
	                               "win.prev");
	nw->bnext = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bnext));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bnext),
	                               "win.next");
	nw->blast = gtk_tool_button_new_from_stock(GTK_STOCK_GOTO_LAST);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->blast));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->blast),
	                               "win.last");
	update_toolbar(nw);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_ALWAYS);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
	                                      GTK_WIDGET(nw->sceditor));

	sc_editor_set_size(nw->sceditor, 640, 1024);
	sc_editor_set_logical_size(nw->sceditor, 640.0, 1024.0);
	sc_editor_set_background(nw->sceditor, 0.9, 0.9, 0.9);
	sc_editor_set_min_border(nw->sceditor, 40.0);
	sc_editor_set_top_frame_editable(nw->sceditor, 1);

	g_signal_connect(G_OBJECT(nw->sceditor), "button-press-event",
	                 G_CALLBACK(button_press_sig), nw);

	gtk_window_set_default_size(GTK_WINDOW(nw->window), 768, 768);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all(nw->window);

	return nw;
}