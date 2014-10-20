/*
 * slide_window.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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

#include "presentation.h"
#include "slide_window.h"
#include "render.h"
#include "frame.h"
#include "slideshow.h"
#include "wrap.h"
#include "notes.h"
#include "pr_clock.h"
#include "slide_sorter.h"
#include "sc_parse.h"
#include "sc_interp.h"
#include "sc_editor.h"


struct _slidewindow
{
	struct presentation *p;
	GtkWidget           *window;
	GtkUIManager        *ui;
	GtkActionGroup      *action_group;

	SCEditor            *sceditor;

	struct menu_pl      *style_menu;
	int                  n_style_menu;

	struct slide        *cur_slide;  /* FIXME: SPOT inside SCEditor */
};


static void add_ui_sig(GtkUIManager *ui, GtkWidget *widget,
                       GtkContainer *container)
{
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	if ( GTK_IS_TOOLBAR(widget) ) {
		gtk_toolbar_set_show_arrow(GTK_TOOLBAR(widget), TRUE);
	}
}

void update_toolbar(SlideWindow *sw)
{
	GtkWidget *d;
	int cur_slide_number;

	d = gtk_ui_manager_get_widget(sw->ui, "/ui/displaywindowtoolbar/first");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(sw->ui, "/ui/displaywindowtoolbar/prev");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(sw->ui, "/ui/displaywindowtoolbar/next");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(sw->ui, "/ui/displaywindowtoolbar/last");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);

	cur_slide_number = slide_number(sw->p, sw->cur_slide);
	if ( cur_slide_number == 0 ) {

		d = gtk_ui_manager_get_widget(sw->ui,
					"/ui/displaywindowtoolbar/first");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);
		d = gtk_ui_manager_get_widget(sw->ui,
					"/ui/displaywindowtoolbar/prev");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);

	}

	if ( cur_slide_number == sw->p->num_slides-1 ) {

		d = gtk_ui_manager_get_widget(sw->ui,
					"/ui/displaywindowtoolbar/next");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);
		d = gtk_ui_manager_get_widget(sw->ui,
					"/ui/displaywindowtoolbar/last");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);

	}


}


/* Inelegance to make furniture selection menus work */
struct menu_pl
{
	SlideWindow *sw;
	char *style_name;
	GtkWidget *widget;
};


static gint add_furniture(GtkWidget *widget, struct menu_pl *pl)
{
	sc_block_append_end(pl->sw->cur_slide->scblocks,
	                    strdup(pl->style_name), NULL, NULL);

	//do_slide_update(pl->p, pl->sw->pc); FIXME

	return 0;
}


static void update_style_menus(SlideWindow *sw)
{
	GtkWidget *menu;
	SCInterpreter *scin;
	struct style_id *styles;
	int i, n_sty;

	/* Free old list */
	for ( i=0; i<sw->n_style_menu; i++ ) {
		gtk_widget_destroy(sw->style_menu[i].widget);
		free(sw->style_menu[i].style_name);
	}
	free(sw->style_menu);

	/* Get the list of styles from the style sheet */
	scin = sc_interp_new(NULL, NULL);
	if ( scin == NULL ) {
		fprintf(stderr, "Failed to set up interpreter.\n");
		return;
	}
	sc_interp_run_stylesheet(scin, sw->p->stylesheet);

	styles = list_styles(scin, &n_sty);
	if ( styles == NULL ) return;

	sc_interp_destroy(scin);

	/* Set up list for next time */
	sw->style_menu = calloc(n_sty, sizeof(struct menu_pl));
	if ( sw->style_menu == NULL ) return;

	/* Add the styles to the "Insert" menu */
	menu = gtk_ui_manager_get_widget(sw->ui, "/displaywindow/insert");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu));

	for ( i=0; i<n_sty; i++ ) {

		GtkWidget *item;

		item = gtk_menu_item_new_with_label(styles[i].friendlyname);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		sw->style_menu[i].sw = sw;
		sw->style_menu[i].widget = item;
		sw->style_menu[i].style_name = styles[i].name;

		g_signal_connect(G_OBJECT(item), "activate",
		                 G_CALLBACK(add_furniture),
		                 &sw->style_menu[i]);

		free(styles[i].friendlyname);
	}

	gtk_widget_show_all(menu);
	free(styles);
}


static gint saveas_response_sig(GtkWidget *d, gint response,
                              SlideWindow *sw)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		if ( save_presentation(sw->p, filename) ) {
			//show_error(sw, "Failed to save presentation");
		}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint saveas_sig(GtkWidget *widget, SlideWindow *sw)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Save Presentation",
	                                GTK_WINDOW(sw->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(saveas_response_sig), sw);

	gtk_widget_show_all(d);

	return 0;
}


static gint save_sig(GtkWidget *widget, SlideWindow *sw)
{
	if ( sw->p->filename == NULL ) {
		return saveas_sig(widget, sw);
	}

	save_presentation(sw->p, sw->p->filename);

	return 0;
}


static gint export_pdf_response_sig(GtkWidget *d, gint response,
                                    SlideWindow *sw)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		if ( export_pdf(sw->p, filename) ) {
			//show_error(sw, "Failed to export as PDF");
		}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint export_pdf_sig(GtkWidget *widget, SlideWindow *sw)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Export PDF",
	                                GTK_WINDOW(sw->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(export_pdf_response_sig), sw);

	gtk_widget_show_all(d);

	return 0;
}


static gint about_sig(GtkWidget *widget, SlideWindow *sw)
{
	GtkWidget *window;

	const gchar *authors[] = {
		"Thomas White <taw@bitwiz.org.uk>",
		NULL
	};

	window = gtk_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(sw->window));

	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(window),
	        "Colloquium");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window),
	        PACKAGE_VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(window),
		"© 2014 Thomas White <taw@bitwiz.org.uk>");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(window),
		"A tiny presentation program");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(window),
		"© 2014 Thomas White <taw@bitwiz.org.uk>\n");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(window),
		"http://www.bitwiz.org.uk/");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(window), authors);

	g_signal_connect(window, "response", G_CALLBACK(gtk_widget_destroy),
			 NULL);

	gtk_widget_show_all(window);

	return 0;
}


static gint start_slideshow_sig(GtkWidget *widget, SlideWindow *sw)
{
	sw->p->slideshow = try_start_slideshow(sw->p);
	return FALSE;
}


/* Change the editor's slide to "np" */
void change_edit_slide(SlideWindow *sw, struct slide *np)
{
	sw->cur_slide = np;

	update_toolbar(sw);

	sc_editor_set_slide(sw->sceditor, np);

	notify_notes_slide_changed(sw->p, np);

	if ( slideshow_linked(sw->p->slideshow) ) {
		change_proj_slide(sw->p->slideshow, np);
	} /* else leave the slideshow alone */
}


static gint add_slide_sig(GtkWidget *widget, SlideWindow *sw)
{
	struct slide *new;
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);

	new = add_slide(sw->p, cur_slide_number+1);
	new->scblocks = sc_block_insert_after(sw->cur_slide->scblocks,
	                                      "slide", NULL, NULL);

	change_edit_slide(sw, new);

	return FALSE;
}


static gint first_slide_sig(GtkWidget *widget, SlideWindow *sw)
{
	change_edit_slide(sw, sw->p->slides[0]);
	return FALSE;
}


static gint prev_slide_sig(GtkWidget *widget, SlideWindow *sw)
{
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);
	if ( cur_slide_number == 0 ) return FALSE;

	change_edit_slide(sw, sw->p->slides[cur_slide_number-1]);

	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, SlideWindow *sw)
{
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);
	if ( cur_slide_number == sw->p->num_slides-1 ) return FALSE;

	change_edit_slide(sw, sw->p->slides[cur_slide_number+1]);

	return FALSE;
}


static gint last_slide_sig(GtkWidget *widget, SlideWindow *sw)
{
	change_edit_slide(sw, sw->p->slides[sw->p->num_slides-1]);

	return FALSE;
}


static gint open_notes_sig(GtkWidget *widget, SlideWindow *sw)
{
	open_notes(sw->p);
	return FALSE;
}


static gint open_clock_sig(GtkWidget *widget, SlideWindow *sw)
{
	open_clock(sw->p);
	return FALSE;
}


static gint open_slidesorter_sig(GtkWidget *widget, SlideWindow *sw)
{
	open_slidesorter(sw->p);
	return FALSE;
}

static gint delete_frame_sig(GtkWidget *widget, SlideWindow *sw)
{
#if 0
	int i;

	delete_subframe(sw->cur_slide, sw->p->selection);
	p->n_selection = 0;

	rerender_slide(p);
	redraw_editor(p);
#endif
/* FIXME: implement */
	return FALSE;
}


static void add_menu_bar(SlideWindow *sw, GtkWidget *vbox)
{
	GError *error = NULL;
	GtkWidget *toolbar;
	GtkWidget *menu;
	GtkWidget *item;

	GtkActionEntry entries[] = {

		{ "FileAction", NULL, "_File", NULL, NULL, NULL },
//		{ "NewAction", GTK_STOCK_NEW, "_New",
//			NULL, NULL, G_CALLBACK(new_sig) },
//		{ "OpenAction", GTK_STOCK_OPEN, "_Open...",
//			NULL, NULL, G_CALLBACK(open_sig) },
//		{ "LoadStyleAction", NULL, "_Load Stylesheet...",
//			NULL, NULL, G_CALLBACK(loadstyle_sig) },
		{ "SaveAction", GTK_STOCK_SAVE, "_Save",
			NULL, NULL, G_CALLBACK(save_sig) },
		{ "SaveAsAction", GTK_STOCK_SAVE_AS, "Save _As...",
			NULL, NULL, G_CALLBACK(saveas_sig) },
//		{ "SaveStyleAction", NULL, "Save St_ylesheet",
//			NULL, NULL, G_CALLBACK(save_ss_sig) },
		{ "ExportPDFAction", NULL, "Export PDF",
			NULL, NULL, G_CALLBACK(export_pdf_sig) },
//		{ "QuitAction", GTK_STOCK_QUIT, "_Quit",
//			NULL, NULL, G_CALLBACK(quit_sig) },

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

	};
	guint n_entries = G_N_ELEMENTS(entries);

	sw->action_group = gtk_action_group_new("mainwindow");
	gtk_action_group_add_actions(sw->action_group, entries, n_entries, sw);

	sw->ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(sw->ui, sw->action_group, 0);
	g_signal_connect(sw->ui, "add_widget", G_CALLBACK(add_ui_sig), vbox);
	if ( gtk_ui_manager_add_ui_from_file(sw->ui,
	     DATADIR"/colloquium/colloquium.ui", &error) == 0 ) {
		fprintf(stderr, "Error loading main window menu bar: %s\n",
			error->message);
		return;
	}

	gtk_window_add_accel_group(GTK_WINDOW(sw->window),
				   gtk_ui_manager_get_accel_group(sw->ui));
	gtk_ui_manager_ensure_update(sw->ui);

	toolbar = gtk_ui_manager_get_widget(sw->ui, "/displaywindowtoolbar");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
	                   gtk_separator_tool_item_new(), -1);

	menu = gtk_ui_manager_get_widget(sw->ui, "/displaywindow/insert");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu));
	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	update_style_menus(sw);
	update_toolbar(sw);
}


void update_titlebar(struct presentation *p)
{
	get_titlebar_string(p);

	if ( p->slidewindow != NULL ) {

		char *title;

		title = malloc(strlen(p->titlebar)+14);
		sprintf(title, "%s - Colloquium", p->titlebar);
		gtk_window_set_title(GTK_WINDOW(p->slidewindow->window), title);
		free(title);

       }

}


SlideWindow *slide_window_open(struct presentation *p, GApplication *app)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *scroll;
	SlideWindow *sw;

	if ( p->slidewindow != NULL ) {
		fprintf(stderr, "Slide window is already open!\n");
		return p->slidewindow;
	}

	sw = calloc(1, sizeof(SlideWindow));
	if ( sw == NULL ) return NULL;

	window = gtk_application_window_new(GTK_APPLICATION(app));
	sw->window = window;
	sw->p = p;
	sw->cur_slide = p->slides[0];

	update_titlebar(p);

//	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(close_sig), p);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	sw->sceditor = sc_editor_new(sw->cur_slide->scblocks, p->stylesheet);
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
	                                      sc_editor_get_widget(sw->sceditor));

	/* Size of SCEditor surface in pixels */
	/* FIXME: Somewhat arbitrary.  Should come from slide itself */
	sc_editor_set_size(sw->sceditor, 1024, 768);
	sc_editor_set_logical_size(sw->sceditor, 1024.0, 768.0);

	add_menu_bar(sw, vbox);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	/* Default size */
	gtk_window_set_default_size(GTK_WINDOW(sw->window), 1024+100, 768+150);
	gtk_window_set_resizable(GTK_WINDOW(sw->window), TRUE);

	gtk_widget_show_all(window);

	return sw;
}
