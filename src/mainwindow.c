/*
 * mainwindow.c
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <assert.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "presentation.h"
#include "mainwindow.h"
#include "render.h"


static void redraw_slide(struct slide *s)
{
	int w, h;

	if ( s->rendered_thumb != NULL ) {
		cairo_surface_destroy(s->rendered_thumb);
	}

	w = s->parent->thumb_slide_width;
	h = (s->parent->slide_height/s->parent->slide_width) * w;
	s->rendered_thumb = render_slide(s, w, h);
	/* FIXME: Request redraw for slide sorter if open */

	/* Is this slide currently open in the editor? */
	if ( s == s->parent->cur_edit_slide ) {

		if ( s->rendered_edit != NULL ) {
			cairo_surface_destroy(s->rendered_edit);
		}

		w = s->parent->edit_slide_width;
		h = (s->parent->slide_height/s->parent->slide_width) * w;
		s->rendered_edit = render_slide(s, w, h);

	}

	/* Is this slide currently being displayed on the projector? */
	if ( s == s->parent->cur_proj_slide ) {

		if ( s->rendered_proj != NULL ) {
			cairo_surface_destroy(s->rendered_proj);
		}

		w = s->parent->proj_slide_width;
		h = (s->parent->slide_height/s->parent->slide_width) * w;
		s->rendered_proj = render_slide(s, w, h);

	}
}


static void add_ui_sig(GtkUIManager *ui, GtkWidget *widget,
                       GtkContainer *container)
{
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	if ( GTK_IS_TOOLBAR(widget) ) {
		gtk_toolbar_set_show_arrow(GTK_TOOLBAR(widget), TRUE);
	}
}


static gint quit_sig(GtkWidget *widget, struct presentation *p)
{
	return 0;
}


/* FIXME (unused) */
static UNUSED void show_error(struct presentation *p, const char *message)
{
	GtkWidget *window;

	window = gtk_message_dialog_new(GTK_WINDOW(p->window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_CLOSE, message);
	gtk_window_set_title(GTK_WINDOW(window), "Error");

	g_signal_connect_swapped(window, "response",
				 G_CALLBACK(gtk_widget_destroy), window);
	gtk_widget_show(window);
}


static void update_toolbar(struct presentation *p)
{
	GtkWidget *d;
	int cur_slide_number;

	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/first");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/prev");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/next");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/last");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);

	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == 0 ) {

		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/first");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);
		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/prev");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);

	}

	if ( cur_slide_number == p->num_slides-1 ) {

		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/next");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);
		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/last");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);

	}


}


static gint open_response_sig(GtkWidget *d, gint response,
                              struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		if ( p->completely_empty ) {

			/* FIXME */
			//if ( load_presentation(p, filename) ) {
			//	show_error(p, "Failed to open presentation");
			//}
			//redraw_slide(p->cur_edit_slide);
			update_toolbar(p);

		} else {

			struct presentation *p;

			/* FIXME */
			p = new_presentation();
			//if ( load_presentation(p, filename) ) {
			//	show_error(p, "Failed to open presentation");
			//} else {
				open_mainwindow(p);
			//}

		}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint open_sig(GtkWidget *widget, struct presentation *p)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Open Presentation",
	                                GTK_WINDOW(p->window),
	                                GTK_FILE_CHOOSER_ACTION_OPEN,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                NULL);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(open_response_sig), p);

	gtk_widget_show_all(d);

	return 0;
}


static gint new_sig(GtkWidget *widget, struct presentation *pnn)
{
	struct presentation *p;

	p = new_presentation();
	if ( p != NULL ) {
		p->cur_edit_slide = add_slide(p, 0);
		p->completely_empty = 1;
		open_mainwindow(p);
	}

	return 0;
}


static gint saveas_response_sig(GtkWidget *d, gint response,
                              struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		/* FIXME */
		//if ( save_presentation(p, filename) ) {
		//	show_error(p, "Failed to save presentation");
		//}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint saveas_sig(GtkWidget *widget, struct presentation *p)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Save Presentation",
	                                GTK_WINDOW(p->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(saveas_response_sig), p);

	gtk_widget_show_all(d);

	return 0;
}


static gint save_sig(GtkWidget *widget, struct presentation *p)
{
	if ( p->filename == NULL ) {
		return saveas_sig(widget, p);
	}

	/* FIXME */
	//save_presentation(p, p->filename);

	return 0;
}


static gint save_ss_response_sig(GtkWidget *d, gint response,
                              struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		/* FIXME */
		//if ( save_stylesheet(p->ss, filename) ) {
		//	show_error(p, "Failed to save style sheet");
		//}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint save_ss_sig(GtkWidget *widget, struct presentation *p)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Save Style sheet",
	                                GTK_WINDOW(p->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(save_ss_response_sig), p);

	gtk_widget_show_all(d);

	return 0;
}


static gint export_pdf_response_sig(GtkWidget *d, gint response,
                                    struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		/* FIXME */
		//if ( export_pdf(p, filename) ) {
		//	show_error(p, "Failed to export as PDF");
		//}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint export_pdf_sig(GtkWidget *widget, struct presentation *p)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Export PDF",
	                                GTK_WINDOW(p->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(export_pdf_response_sig), p);

	gtk_widget_show_all(d);

	return 0;
}


static gint about_sig(GtkWidget *widget, struct presentation *p)
{
	GtkWidget *window;

	const gchar *authors[] = {
		"Thomas White <taw@bitwiz.org.uk>",
		NULL
	};

	window = gtk_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(p->window));

	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(window),
	        "Colloquium");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window),
	        PACKAGE_VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(window),
		"(c) 2012 Thomas White <taw@bitwiz.org.uk>");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(window),
		"A tiny presentation program");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(window),
		"(c) 2012 Thomas White <taw@bitwiz.org.uk>\n");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(window),
		"http://www.bitwiz.org.uk/");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(window), authors);

	g_signal_connect(window, "response", G_CALLBACK(gtk_widget_destroy),
			 NULL);

	gtk_widget_show_all(window);

	return 0;
}


static gint start_slideshow_sig(GtkWidget *widget, struct presentation *p)
{
	/* FIXME */
	//try_start_slideshow(p);
	return FALSE;
}


void notify_slide_changed(struct presentation *p, struct slide *np)
{
	p->cur_edit_slide = np;

	/* FIXME: Free old rendered stuff */

	update_toolbar(p);
	redraw_slide(p->cur_edit_slide);

	if ( p->notes != NULL ) {
		//notify_notes_slide_changed(p, np);
	}

	if ( (p->slideshow != NULL) && p->slideshow_linked ) {
		//notify_slideshow_slide_changed(p, np);
	}
}


static gint add_slide_sig(GtkWidget *widget, struct presentation *p)
{
	struct slide *new;
	int cur_slide_number;

	cur_slide_number = slide_number(p, p->cur_edit_slide);

	new = add_slide(p, cur_slide_number);
	notify_slide_changed(p, new);

	return FALSE;
}


static gint first_slide_sig(GtkWidget *widget, struct presentation *p)
{
	notify_slide_changed(p, p->slides[0]);
	return FALSE;
}


static gint prev_slide_sig(GtkWidget *widget, struct presentation *p)
{
	int cur_slide_number;

	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == 0 ) return FALSE;

	notify_slide_changed(p, p->slides[cur_slide_number-1]);

	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, struct presentation *p)
{
	int cur_slide_number;

	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == p->num_slides-1 ) return FALSE;

	notify_slide_changed(p, p->slides[cur_slide_number+1]);

	return FALSE;
}


static gint last_slide_sig(GtkWidget *widget, struct presentation *p)
{
	notify_slide_changed(p, p->slides[p->num_slides-1]);

	return FALSE;
}


static gint open_stylesheet_sig(GtkWidget *widget, struct presentation *p)
{
	/* FIXME */
	//if ( p->stylesheetwindow == NULL ) {
	//	p->stylesheetwindow = open_stylesheet(p);
	//} /* else already open */

	return FALSE;
}


static gint open_notes_sig(GtkWidget *widget, struct presentation *p)
{
	/* FIXME */
	//open_notes(p);
	return FALSE;
}


static void add_menu_bar(struct presentation *p, GtkWidget *vbox)
{
	GError *error = NULL;
	GtkWidget *toolbar;
	GtkWidget *menu;
	GtkWidget *item;
	//int i;
	GtkActionEntry entries[] = {

		{ "FileAction", NULL, "_File", NULL, NULL, NULL },
		{ "NewAction", GTK_STOCK_NEW, "_New",
			NULL, NULL, G_CALLBACK(new_sig) },
		{ "OpenAction", GTK_STOCK_OPEN, "_Open...",
			NULL, NULL, G_CALLBACK(open_sig) },
		{ "SaveAction", GTK_STOCK_SAVE, "_Save",
			NULL, NULL, G_CALLBACK(save_sig) },
		{ "SaveAsAction", GTK_STOCK_SAVE_AS, "Save _As...",
			NULL, NULL, G_CALLBACK(saveas_sig) },
		{ "SaveStyleAction", GTK_STOCK_SAVE_AS, "Save St_ylesheet",
			NULL, NULL, G_CALLBACK(save_ss_sig) },
		{ "ExportPDFAction", GTK_STOCK_SAVE_AS, "Export PDF",
			NULL, NULL, G_CALLBACK(export_pdf_sig) },
		{ "QuitAction", GTK_STOCK_QUIT, "_Quit",
			NULL, NULL, G_CALLBACK(quit_sig) },

		{ "EditAction", NULL, "_Edit", NULL, NULL, NULL },
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
		{ "DeleteAction", GTK_STOCK_DELETE, "Delete",
			NULL, NULL, NULL },
		{ "EditStyleAction", NULL, "Stylesheet...",
			NULL, NULL, G_CALLBACK(open_stylesheet_sig) },

		{ "InsertAction", NULL, "_Insert", NULL, NULL, NULL },
		{ "NewSlideAction", GTK_STOCK_ADD, "_New Slide",
			NULL, NULL, G_CALLBACK(add_slide_sig) },

		{ "ToolsAction", NULL, "_Tools", NULL, NULL, NULL },
		{ "TSlideshowAction", GTK_STOCK_FULLSCREEN, "_Start slideshow",
			"F5", NULL, G_CALLBACK(start_slideshow_sig) },
		{ "NotesAction", NULL, "_Open slide notes",
			"F5", NULL, G_CALLBACK(open_notes_sig) },
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

	p->action_group = gtk_action_group_new("mainwindow");
	gtk_action_group_add_actions(p->action_group, entries, n_entries, p);

	p->ui = gtk_ui_manager_new();
	gtk_ui_manager_insert_action_group(p->ui, p->action_group, 0);
	g_signal_connect(p->ui, "add_widget", G_CALLBACK(add_ui_sig), vbox);
	if ( gtk_ui_manager_add_ui_from_file(p->ui,
	     DATADIR"/colloquium/colloquium.ui", &error) == 0 ) {
		fprintf(stderr, "Error loading main window menu bar: %s\n",
			error->message);
		return;
	}

	gtk_window_add_accel_group(GTK_WINDOW(p->window),
				   gtk_ui_manager_get_accel_group(p->ui));
	gtk_ui_manager_ensure_update(p->ui);

	toolbar = gtk_ui_manager_get_widget(p->ui, "/displaywindowtoolbar");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar),
	                   gtk_separator_tool_item_new(), -1);

	p->tbox = GTK_WIDGET(gtk_tool_item_new());
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(p->tbox), -1);

	/* Add the styles to the "Insert" menu */
	menu = gtk_ui_manager_get_widget(p->ui, "/displaywindow/insert");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu));
	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* FIXME */
	//for ( i=1; i<p->ss->n_styles; i++ )
	//{
	//	char *name;
	//	name = p->ss->styles[i]->name;
	//	item = gtk_menu_item_new_with_label(name);
	//	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	//	g_signal_connect(G_OBJECT(item), "activate",
	//                       G_CALLBACK(add_furniture), p);
	//
	//}

	update_toolbar(p);
}


static gint close_sig(GtkWidget *window, struct presentation *p)
{
	free_presentation(p);
	return 0;
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr,
                         struct presentation *p)
{
	double xoff, yoff;
	int width, height;

	width = gtk_widget_get_allocated_width(GTK_WIDGET(da));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(da));

	/* Overall background */
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	if ( (p->slideshow != NULL) && !p->slideshow_linked  ) {
		cairo_set_source_rgb(cr, 1.0, 0.3, 0.2);
	} else {
		cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	}
	cairo_fill(cr);

	/* Get the overall size */
	xoff = (width - p->slide_width)/2.0;
	yoff = (height - p->slide_height)/2.0;
	p->border_offs_x = xoff;  p->border_offs_y = yoff;
	
	/* Draw the slide from the cache */
	cairo_rectangle(cr, xoff, yoff, p->slide_width, p->slide_height);
	cairo_set_source_surface(cr, p->cur_edit_slide->rendered_edit,
	                         xoff, yoff);
	cairo_fill(cr);

	cairo_translate(cr, xoff, yoff);

	/* FIXME */
	//draw_overlay(cr, p);

	return FALSE;
}


void update_titlebar(struct presentation *p)
{
	get_titlebar_string(p);

	if ( p->window != NULL ) {

		char *title;

		title = malloc(strlen(p->titlebar)+14);
		sprintf(title, "%s - Colloquium", p->titlebar);
		gtk_window_set_title(GTK_WINDOW(p->window), title);
		free(title);

	}
}


int open_mainwindow(struct presentation *p)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *sw;

	if ( p->window != NULL ) {
		fprintf(stderr, "Presentation window is already open!\n");
		return 1;
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	p->window = window;

	update_titlebar(p);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(close_sig), p);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	p->drawingarea = gtk_drawing_area_new();
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw),
	                                      p->drawingarea);
	gtk_widget_set_size_request(GTK_WIDGET(p->drawingarea),
	                            p->slide_width + 20,
	                            p->slide_height + 20);

	add_menu_bar(p, vbox);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	gtk_widget_set_can_focus(GTK_WIDGET(p->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(p->drawingarea),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	g_signal_connect(G_OBJECT(p->drawingarea), "draw",
			 G_CALLBACK(draw_sig), p);

	/* Input method */
	/* FIXME */
	p->im_context = gtk_im_multicontext_new();
	//gtk_im_context_set_client_window(GTK_IM_CONTEXT(p->im_context),
	//                                 p->drawingarea->window);
	//g_signal_connect(G_OBJECT(p->im_context), "commit",
	//		 G_CALLBACK(im_commit_sig), p);

	/* Default size */
	gtk_window_set_default_size(GTK_WINDOW(p->window), 1024+100, 768+100);
	gtk_window_set_resizable(GTK_WINDOW(p->window), TRUE);

	assert(p->num_slides > 0);

	gtk_widget_grab_focus(GTK_WIDGET(p->drawingarea));

	gtk_widget_show_all(window);

	p->edit_slide_width = 1024;
	p->proj_slide_width = 2048;
	p->thumb_slide_width = 320;  /* FIXME: Completely made up */

	redraw_slide(p->cur_edit_slide);

	return 0;
}
