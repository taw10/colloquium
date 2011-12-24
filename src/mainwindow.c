/*
 * mainwindow.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2011 Thomas White <taw@bitwiz.org.uk>
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
#include "slide_render.h"
#include "objects.h"
#include "slideshow.h"
#include "stylesheet.h"
#include "loadsave.h"
#include "tool_select.h"
#include "tool_text.h"
#include "tool_image.h"


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


static void show_error(struct presentation *p, const char *message)
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

			if ( load_presentation(p, filename) ) {
				show_error(p, "Failed to open presentation");
			}
			redraw_slide(p->cur_edit_slide);
			update_toolbar(p);

		} else {

			struct presentation *p;

			p = new_presentation();
			if ( load_presentation(p, filename) ) {
				show_error(p, "Failed to open presentation");
			} else {
				open_mainwindow(p);
			}

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

		if ( save_presentation(p, filename) ) {
			show_error(p, "Failed to save presentation");
		}

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

	save_presentation(p, p->filename);

	return 0;
}


static gint save_ss_response_sig(GtkWidget *d, gint response,
                              struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		if ( save_stylesheet(p->ss, filename) ) {
			show_error(p, "Failed to save style sheet");
		}

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

		if ( export_pdf(p, filename) ) {
			show_error(p, "Failed to export as PDF");
		}

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

	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(window), "Colloquium");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window), PACKAGE_VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(window),
		"(c) 2011 Thomas White <taw@bitwiz.org.uk>");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(window),
		"A tiny presentation program");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(window),
		"(c) 2011 Thomas White <taw@bitwiz.org.uk>\n");
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
	try_start_slideshow(p);
	return FALSE;
}


void notify_slide_changed(struct presentation *p, struct slide *np)
{
	p->cur_tool->deselect(p->editing_object, p->cur_tool);
	p->editing_object = NULL;

	if ( p->cur_edit_slide->rendered_edit != NULL ) {
		cairo_surface_destroy(p->cur_edit_slide->rendered_edit);
		p->cur_edit_slide->rendered_edit = NULL;
	}
	p->cur_edit_slide = np;

	update_toolbar(p);
	redraw_slide(p->cur_edit_slide);

	if ( (p->slideshow != NULL) && p->slideshow_linked ) {
		notify_slideshow_slide_changed(p, np);
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
	if ( p->stylesheetwindow == NULL ) {
		p->stylesheetwindow = open_stylesheet(p);
	} /* else already open */

	return FALSE;
}


enum tool_id
{
	TOOL_SELECT,
	TOOL_TEXT,
	TOOL_IMAGE,
};


static gint set_tool_sig(GtkWidget *widget, GtkRadioAction *action,
                         struct presentation *p)
{
	if ( p->editing_object != NULL ) {
		int d = p->cur_tool->deselect(p->editing_object, p->cur_tool);
		if ( d ) p->editing_object = NULL;
	}

	switch ( gtk_radio_action_get_current_value(action) )
	{
		case TOOL_SELECT : p->cur_tool = p->select_tool; break;
		case TOOL_TEXT : p->cur_tool = p->text_tool; break;
		case TOOL_IMAGE : p->cur_tool = p->image_tool; break;
	}

	gtk_container_remove(GTK_CONTAINER(p->tbox), p->cur_tbox);
	p->cur_tbox = p->cur_tool->tbox;
	gtk_container_add(GTK_CONTAINER(p->tbox), p->cur_tbox);

	if ( p->editing_object != NULL ) {
		if ( p->cur_tool->valid_object(p->editing_object) ) {
			p->cur_tool->select(p->editing_object, p->cur_tool);
		} else {
			p->editing_object = NULL;
		}
	}

	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	return 0;
}


static void force_tool(struct presentation *p, enum tool_id tool)
{
	GtkAction *action;
	action = gtk_ui_manager_get_action(p->ui,
	                                   "/ui/displaywindowtoolbar/select");
	gtk_radio_action_set_current_value(GTK_RADIO_ACTION(action), tool);
}


static gint add_furniture(GtkWidget *widget, struct presentation *p)
{
	gchar *name;
	struct style *sty;

	g_object_get(G_OBJECT(widget), "label", &name, NULL);
	sty = find_style(p->ss, name);
	g_free(name);
	if ( sty == NULL ) return 0;

	force_tool(p, TOOL_TEXT);
	p->text_tool->create_default(p, sty, p->cur_edit_slide, p->text_tool);

	return 0;
}


static void add_menu_bar(struct presentation *p, GtkWidget *vbox)
{
	GError *error = NULL;
	GtkWidget *toolbar;
	GtkWidget *menu;
	GtkWidget *item;
	int i;
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
	GtkRadioActionEntry tools[] = {
		{ "ButtonToolSelectAction", "colloquium-select", "Select",
			NULL, NULL, TOOL_SELECT },
		{ "ButtonToolTextAction", "colloquium-text", "Text",
			NULL, NULL, TOOL_TEXT },
		{ "ButtonToolImageAction", "colloquium-image", "Image",
			NULL, NULL, TOOL_IMAGE },
	};
	guint n_tools = G_N_ELEMENTS(tools);

	p->action_group = gtk_action_group_new("mainwindow");
	gtk_action_group_add_actions(p->action_group, entries, n_entries, p);
	gtk_action_group_add_radio_actions(p->action_group, tools, n_tools,
	                                   TOOL_SELECT,
	                                   G_CALLBACK(set_tool_sig), p);

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
	for ( i=1; i<p->ss->n_styles; i++ )
	{
		char *name;
		name = p->ss->styles[i]->name;
		item = gtk_menu_item_new_with_label(name);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
	                         G_CALLBACK(add_furniture), p);

	}
	p->cur_tbox = NULL;
	p->cur_tool = p->select_tool;
	p->cur_tbox = p->cur_tool->tbox;
	gtk_container_add(GTK_CONTAINER(p->tbox), p->cur_tbox);
	update_toolbar(p);
}


static gint close_sig(GtkWidget *window, struct presentation *p)
{
	free_presentation(p);
	return 0;
}


static void redraw_object(struct object *o)
{
	if ( o == NULL ) return;
	gdk_window_invalidate_rect(o->parent->parent->drawingarea->window,
	                           NULL, FALSE);
}


void redraw_overlay(struct presentation *p)
{
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              struct presentation *p)
{
	if ( p->editing_object == NULL ) {
		printf("IM keypress: %s\n", str);
		return FALSE;
	}
	if ( p->editing_object->type != OBJ_TEXT ) return FALSE;

	p->cur_tool->im_commit(p->editing_object, str, p->cur_tool);

	redraw_object(p->editing_object);

	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              struct presentation *p)
{
	gboolean r;

	/* FIXME: Only if doing sensible text input */
	if ( p->editing_object != NULL ) {

		/* Throw the event to the IM context and let it sort things
		 * out */
		r = gtk_im_context_filter_keypress(
		                                  GTK_IM_CONTEXT(p->im_context),
			                          event);
		if ( r ) return FALSE;  /* IM ate it */

	}

	p->cur_tool->key_pressed(p->editing_object, event->keyval, p->cur_tool);

	switch ( event->keyval ) {

		case GDK_KEY_Page_Up :
		prev_slide_sig(NULL, p);
		break;

		case GDK_KEY_Page_Down :
		next_slide_sig(NULL, p);
		break;

		case GDK_KEY_Escape :
		if ( p->slideshow != NULL ) end_slideshow(p);
		redraw_object(p->editing_object);
		p->editing_object = NULL;
		break;

		case GDK_KEY_B :
		case GDK_KEY_b :
		if ( p->slideshow != NULL ) {
			if ( p->prefs->b_splits ) {
				toggle_slideshow_link(p);
			} else {
				p->ss_blank = 1-p->ss_blank;
				gdk_window_invalidate_rect(
				                      p->ss_drawingarea->window,
					             NULL, FALSE);
			}
		}
		break;

	}

	return FALSE;
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 struct presentation *p)
{
	struct object *clicked;
	gdouble x, y;

	x = event->x - p->border_offs_x;
	y = event->y - p->border_offs_y;

	clicked = find_object_at_position(p->cur_edit_slide, x, y);

	if ( clicked == NULL ) {

		/* Clicked no object. Deselect old object and set up for
		 * (maybe) creating a new one. */

		if ( p->editing_object != NULL ) {
			p->cur_tool->deselect(p->editing_object, p->cur_tool);
			p->editing_object = NULL;
		}
		p->start_corner_x = event->x - p->border_offs_x;
		p->start_corner_y = event->y - p->border_offs_y;
		p->drag_status = DRAG_STATUS_COULD_DRAG;
		p->drag_reason = DRAG_REASON_CREATE;

	} else {

		/* If the clicked object is not the same as the previously
		 * selected one, deselect the old one. */
		if ( p->editing_object != clicked ) {
			p->cur_tool->deselect(p->editing_object, p->cur_tool);
			p->editing_object = NULL;
		}

		p->drag_status = DRAG_STATUS_NONE;
		p->drag_reason = DRAG_REASON_NONE;

		if ( p->cur_tool->valid_object(clicked) ) {
			p->editing_object = clicked;
			p->cur_tool->click_select(p, p->cur_tool, x, y, event,
			                          &p->drag_status,
			                          &p->drag_reason);
		}

	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw_overlay(p);
	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                           struct presentation *p)
{
	if ( p->drag_status == DRAG_STATUS_COULD_DRAG ) {

		/* We just got a motion signal, and the status was "could drag",
		 * therefore the drag has started. */
		p->drag_status = DRAG_STATUS_DRAGGING;

	}

	switch ( p->drag_reason ) {

	case DRAG_REASON_NONE :
		break;

	case DRAG_REASON_CREATE :
		p->drag_corner_x = event->x - p->border_offs_x;
		p->drag_corner_y = event->y - p->border_offs_y;
		redraw_overlay(p);
		break;

	case DRAG_REASON_IMPORT :
		/* Do nothing, handled by dnd_motion() */
		break;

	case DRAG_REASON_TOOL :
		p->cur_tool->drag(p->cur_tool, p, p->editing_object,
		                  event->x - p->border_offs_x,
		                  event->y - p->border_offs_y);
		break;

	}

	gdk_event_request_motions(event);
	return FALSE;
}


static gboolean button_release_sig(GtkWidget *da, GdkEventButton *event,
                                   struct presentation *p)
{
	gdouble x, y;

	x = event->x - p->border_offs_x;
	y = event->y - p->border_offs_y;

	/* Not dragging?  Then I don't care. */
	if ( p->drag_status != DRAG_STATUS_DRAGGING ) return FALSE;

	p->drag_corner_x = x;
	p->drag_corner_y = y;
	p->drag_status = DRAG_STATUS_NONE;

	switch ( p->drag_reason )
	{

	case DRAG_REASON_NONE :
		printf("Release on pointless drag.\n");
		break;

	case DRAG_REASON_CREATE :
		redraw_overlay(p);
		p->cur_tool->create_region(p->cur_tool, p,
					   p->start_corner_x,
					   p->start_corner_y,
					   p->drag_corner_x,
					   p->drag_corner_y);
		break;

	case DRAG_REASON_IMPORT :
		/* Do nothing, handled in dnd_drop() or dnd_leave() */
		break;

	case DRAG_REASON_TOOL :
		p->cur_tool->end_drag(p->cur_tool, p, p->editing_object, x, y);
		break;


	}

	p->drag_reason = DRAG_REASON_NONE;

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw_overlay(p);
	return FALSE;
}


static void draw_overlay(cairo_t *cr, struct presentation *p)
{
	struct object *o = p->editing_object;

	p->cur_tool->draw_editing_overlay(p->cur_tool, cr, o);

	if ( o != NULL ) {
		/* Draw margins */
		cairo_move_to(cr, o->style->margin_left, -p->border_offs_y);
		cairo_line_to(cr, o->style->margin_left,
			          p->slide_height+p->border_offs_y);

		cairo_move_to(cr, p->slide_width-o->style->margin_right,
			          -p->border_offs_y);
		cairo_line_to(cr, p->slide_width-o->style->margin_right,
			          p->slide_height+p->border_offs_y);

		cairo_move_to(cr, -p->border_offs_x, o->style->margin_top);
		cairo_line_to(cr, p->slide_width+p->border_offs_x,
			          o->style->margin_top);

		cairo_move_to(cr, -p->border_offs_x,
			          p->slide_height-o->style->margin_bottom);
		cairo_line_to(cr, p->slide_width+p->border_offs_x,
			          p->slide_height-o->style->margin_bottom);

		cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);
	}

	if ( (p->drag_status == DRAG_STATUS_DRAGGING)
	  && ((p->drag_reason == DRAG_REASON_CREATE)
	      || (p->drag_reason == DRAG_REASON_IMPORT)) )
	{
		cairo_new_path(cr);
		cairo_rectangle(cr, p->start_corner_x, p->start_corner_y,
		                    p->drag_corner_x - p->start_corner_x,
		                    p->drag_corner_y - p->start_corner_y);
		cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
		cairo_set_line_width(cr, 0.5);
		cairo_stroke(cr);
	}

	if ( p->slideshow_linked ) {

	}
}


static gboolean expose_sig(GtkWidget *da, GdkEventExpose *event,
                           struct presentation *p)
{
	cairo_t *cr;
	GtkAllocation allocation;
	double xoff, yoff;

	cr = gdk_cairo_create(da->window);

	/* Overall background */
	cairo_rectangle(cr, event->area.x, event->area.y,
	                event->area.width, event->area.height);
	if ( (p->slideshow != NULL) && !p->slideshow_linked  ) {
		cairo_set_source_rgb(cr, 1.0, 0.3, 0.2);
	} else {
		cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	}
	cairo_fill(cr);

	/* Get the overall size */
	gtk_widget_get_allocation(da, &allocation);
	xoff = (allocation.width - p->slide_width)/2.0;
	yoff = (allocation.height - p->slide_height)/2.0;
	p->border_offs_x = xoff;  p->border_offs_y = yoff;

	/* Draw the slide from the cache */
	cairo_rectangle(cr, event->area.x, event->area.y,
	                event->area.width, event->area.height);
	cairo_set_source_surface(cr, p->cur_edit_slide->rendered_edit,
	                         xoff, yoff);
	cairo_fill(cr);

	cairo_translate(cr, xoff, yoff);

	draw_overlay(cr, p);

	cairo_destroy(cr);

	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, struct presentation *p)
{
	GdkAtom target;

	/* If we haven't already requested the data, do so now */
	if ( !p->drag_preview_pending && !p->have_drag_data ) {
		target = gtk_drag_dest_find_target(widget, drag_context, NULL);
		gtk_drag_get_data(widget, drag_context, target, time);
		p->drag_preview_pending = 1;
	}

	if ( p->have_drag_data && p->import_acceptable ) {

		struct style *sty;
		double eright, ebottom;

		gdk_drag_status(drag_context, GDK_ACTION_LINK, time);
		p->start_corner_x = x - p->import_width/2.0;
		p->start_corner_y = y - p->import_height/2.0;
		p->drag_corner_x = x + p->import_width/2.0;
		p->drag_corner_y = y + p->import_height/2.0;

		sty = p->ss->styles[0];

		eright = p->slide_width - sty->margin_right;
		ebottom = p->slide_height - sty->margin_bottom;
		if ( p->start_corner_x < sty->margin_left ) {
			p->start_corner_x = sty->margin_left;
			p->drag_corner_x = sty->margin_left + p->import_width;
		}
		if ( p->start_corner_y < sty->margin_top ) {
			p->start_corner_y = sty->margin_top;
			p->drag_corner_y = sty->margin_top + p->import_height;
		}
		if ( p->drag_corner_x > eright ) {
			p->drag_corner_x = eright;
			p->start_corner_x = eright - p->import_width;
		}
		if ( p->drag_corner_y > ebottom ) {
			p->drag_corner_y = ebottom;
			p->start_corner_y = ebottom - p->import_height;
		}

		redraw_overlay(p);

	}

	return TRUE;
}


static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, struct presentation *p)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target(widget, drag_context, NULL);
	gtk_drag_get_data(widget, drag_context, target, time);

	return TRUE;
}


static void chomp(char *s)
{
	size_t i;

	if ( !s ) return;

	for ( i=0; i<strlen(s); i++ ) {
		if ( (s[i] == '\n') || (s[i] == '\r') ) {
			s[i] = '\0';
			return;
		}
	}
}


/* Scale the image down if it's a silly size */
static void check_import_size(struct presentation *p)
{
	if ( p->import_width > p->slide_width ) {

		int new_import_width;

		new_import_width = p->slide_width/2;
		p->import_height = (new_import_width *p->import_height)
		                     / p->import_width;
		p->import_width = new_import_width;
	}

	if ( p->import_height > p->slide_height ) {

		int new_import_height;

		new_import_height = p->slide_height/2;
		p->import_width = (new_import_height*p->import_width)
		                    / p->import_height;
		p->import_height = new_import_height;
	}
}


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, struct presentation *p)
{
	if ( p->drag_preview_pending ) {

		gchar *filename;
		GError *error = NULL;
		GdkPixbufFormat *f;
		const gchar *uri;
		int w, h;

		p->have_drag_data = 1;
		p->drag_preview_pending = 0;
		uri = (gchar *)seldata->data;

		filename = g_filename_from_uri(uri, NULL, &error);
		if ( filename == NULL ) {

			/* This doesn't even look like a sensible URI.
			 * Bail out. */
			gdk_drag_status(drag_context, 0, time);
			if ( p->drag_highlight ) {
				gtk_drag_unhighlight(widget);
				p->drag_highlight = 0;
			}
			p->import_acceptable = 0;
			return;

		}
		chomp(filename);

		f = gdk_pixbuf_get_file_info(filename, &w, &h);
		g_free(filename);

		p->import_width = w;
		p->import_height = h;

		if ( f == NULL ) {

			gdk_drag_status(drag_context, 0, time);
			if ( p->drag_highlight ) {
				gtk_drag_unhighlight(widget);
				p->drag_highlight = 0;
			}
			p->drag_status = DRAG_STATUS_NONE;
			p->drag_reason = DRAG_REASON_NONE;
			p->import_acceptable = 0;

		} else {

			/* Looks like a sensible image */
			gdk_drag_status(drag_context, GDK_ACTION_LINK, time);
			p->import_acceptable = 1;

			if ( !p->drag_highlight ) {
				gtk_drag_highlight(widget);
				p->drag_highlight = 1;
			}

			check_import_size(p);
			p->drag_reason = DRAG_REASON_IMPORT;
			p->drag_status = DRAG_STATUS_DRAGGING;

		}

	} else {

		gchar *uri;
		char *filename;
		GError *error = NULL;

		uri = (gchar *)seldata->data;

		filename = g_filename_from_uri(uri, NULL, &error);
		if ( filename != NULL ) {

			struct object *o;

			gtk_drag_finish(drag_context, TRUE, FALSE, time);
			chomp(filename);
			o = add_image_object(p->cur_edit_slide,
				             p->start_corner_x,
				             p->start_corner_y,
				             p->import_width, p->import_height,
				             filename,
				             p->ss->styles[0], p->image_store,
				             p->image_tool);

			force_tool(p, TOOL_IMAGE);
			p->editing_object = o;
			redraw_object(o);

			free(filename);
		}

	}
}


static void dnd_leave(GtkWidget *widget, GdkDragContext *drag_context,
                      guint time, struct presentation *p)
{
	if ( p->drag_highlight ) {
		gtk_drag_unhighlight(widget);
	}
	p->have_drag_data = 0;
	p->drag_highlight = 0;
	p->drag_status = DRAG_STATUS_NONE;
	p->drag_reason = DRAG_REASON_NONE;
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
	GtkTargetEntry targets[1];

	if ( p->window != NULL ) {
		fprintf(stderr, "Presentation window is already open!\n");
		return 1;
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	p->window = window;

	update_titlebar(p);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(close_sig), p);

	vbox = gtk_vbox_new(FALSE, 0);
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

	realise_everything(p);
	add_menu_bar(p, vbox);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	gtk_widget_set_can_focus(GTK_WIDGET(p->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(p->drawingarea),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	g_signal_connect(G_OBJECT(p->drawingarea), "button-press-event",
			 G_CALLBACK(button_press_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "button-release-event",
			 G_CALLBACK(button_release_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "key-press-event",
			 G_CALLBACK(key_press_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "expose-event",
			 G_CALLBACK(expose_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "motion-notify-event",
			 G_CALLBACK(motion_sig), p);

	/* Drag and drop */
	targets[0].target = "text/uri-list";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(p->drawingarea, 0, targets, 1, GDK_ACTION_LINK);
	g_signal_connect(p->drawingarea, "drag-data-received",
	                 G_CALLBACK(dnd_receive), p);
	g_signal_connect(p->drawingarea, "drag-motion",
	                 G_CALLBACK(dnd_motion), p);
	g_signal_connect(p->drawingarea, "drag-drop",
	                 G_CALLBACK(dnd_drop), p);
	g_signal_connect(p->drawingarea, "drag-leave",
	                 G_CALLBACK(dnd_leave), p);

	/* Input method */
	p->im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(p->im_context),
	                                 p->drawingarea->window);
	g_signal_connect(G_OBJECT(p->im_context), "commit",
			 G_CALLBACK(im_commit_sig), p);

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
