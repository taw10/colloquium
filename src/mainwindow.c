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

#include "presentation.h"
#include "mainwindow.h"
#include "slide_render.h"
#include "objects.h"
#include "slideshow.h"
#include "stylesheet.h"
#include "loadsave.h"
#include "tool_select.h"
#include "tool_text.h"


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


static gint open_response_sig(GtkWidget *d, gint response,
                              struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));

		if ( p->completely_empty ) {
			if ( load_presentation(p, filename) ) {
				show_error(p, "Failed to open presentation");
			} else {
				open_mainwindow(p);
			}
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


static gint save_ss_sig(GtkWidget *widget, struct presentation *p)
{
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


static void update_toolbar(struct presentation *p)
{
	GtkWidget *d;

	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/first");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/prev");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/next");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);
	d = gtk_ui_manager_get_widget(p->ui, "/ui/displaywindowtoolbar/last");
	gtk_widget_set_sensitive(GTK_WIDGET(d), TRUE);

	if ( p->view_slide_number == 0 ) {

		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/first");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);
		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/prev");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);

	}

	if ( p->view_slide_number == p->num_slides-1 ) {

		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/next");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);
		d = gtk_ui_manager_get_widget(p->ui,
					"/ui/displaywindowtoolbar/last");
		gtk_widget_set_sensitive(GTK_WIDGET(d), FALSE);

	}


}


static gint start_slideshow_sig(GtkWidget *widget, struct presentation *p)
{
	try_start_slideshow(p);
	return FALSE;
}


void notify_slide_changed(struct presentation *p)
{
	p->editing_object = NULL;
	update_toolbar(p);
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	if ( p->slideshow != NULL ) {
		notify_slideshow_slide_changed(p);
	}
}


void notify_slide_update(struct presentation *p)
{
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	if ( p->slideshow != NULL ) {
		notify_slideshow_slide_changed(p);
	}
}


static gint add_slide_sig(GtkWidget *widget, struct presentation *p)
{
	struct slide *new;

	new = add_slide(p, p->view_slide_number);
	p->view_slide = new;
	p->view_slide_number++;
	notify_slide_changed(p);

	return FALSE;
}


static gint first_slide_sig(GtkWidget *widget, struct presentation *p)
{
	p->view_slide_number = 0;
	p->view_slide = p->slides[0];
	notify_slide_changed(p);

	return FALSE;
}


static gint prev_slide_sig(GtkWidget *widget, struct presentation *p)
{
	if ( p->view_slide_number == 0 ) return FALSE;

	p->view_slide_number--;
	p->view_slide = p->slides[p->view_slide_number];
	notify_slide_changed(p);

	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, struct presentation *p)
{
	if ( p->view_slide_number == p->num_slides-1 ) return FALSE;

	p->view_slide_number++;
	p->view_slide = p->slides[p->view_slide_number];
	notify_slide_changed(p);

	return FALSE;
}


static gint last_slide_sig(GtkWidget *widget, struct presentation *p)
{
	p->view_slide_number = p->num_slides-1;
	p->view_slide = p->slides[p->view_slide_number];
	notify_slide_changed(p);

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
	}

	if ( p->editing_object != NULL ) {
		p->cur_tool->select(p->editing_object, p->cur_tool);
	}

	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	return 0;
}


static gint add_furniture(GtkWidget *widget, struct presentation *p)
{
	gchar *name;
	struct style *sty;

	g_object_get(G_OBJECT(widget), "label", &name, NULL);
	sty = find_style(p->ss, name);
	g_free(name);
	if ( sty == NULL ) return 0;

	p->text_tool->create_default(p, sty, p->text_tool);

	return 0;
}


static void add_menu_bar(struct presentation *p, GtkWidget *vbox)
{
	GError *error = NULL;
	GtkToolItem *titem;
	GtkWidget *toolbar;
	GtkWidget *menu;
	GtkWidget *item;
	int i;
	GtkActionEntry entries[] = {

		{ "FileAction", NULL, "_File", NULL, NULL, NULL },
		{ "NewAction", GTK_STOCK_NEW, "_New",
			NULL, NULL, NULL },
		{ "OpenAction", GTK_STOCK_OPEN, "_Open...",
			NULL, NULL, G_CALLBACK(open_sig) },
		{ "SaveAction", GTK_STOCK_SAVE, "_Save",
			NULL, NULL, G_CALLBACK(save_sig) },
		{ "SaveAsAction", GTK_STOCK_SAVE_AS, "Save _As...",
			NULL, NULL, G_CALLBACK(saveas_sig) },
		{ "SaveStyleAction", GTK_STOCK_SAVE_AS, "Save St_ylesheet",
			NULL, NULL, G_CALLBACK(save_ss_sig) },
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
	titem = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), titem, -1);

	p->tbox = gtk_hbox_new(FALSE, 0.0);
	titem = gtk_tool_item_new();
	gtk_container_add(GTK_CONTAINER(titem), p->tbox);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), titem, -1);

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
	update_toolbar(p);
}


static gint close_sig(GtkWidget *window, struct presentation *p)
{
	gtk_main_quit();
	return 0;
}


static void redraw_object(struct object *o)
{
	if ( o == NULL ) return;
	gdk_window_invalidate_rect(o->parent->parent->drawingarea->window,
	                           NULL, FALSE);
}


static void redraw_overlay(struct presentation *p)
{
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              struct presentation *p)
{
	if ( p->editing_object == NULL ) return FALSE;
	if ( p->editing_object->type != TEXT ) return FALSE;

	insert_text(p->editing_object, str);

	redraw_object(p->editing_object);

	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              struct presentation *p)
{
	gboolean r;

	/* Throw the event to the IM context and let it sort things out */
	r = gtk_im_context_filter_keypress(GTK_IM_CONTEXT(p->im_context),
	                                   event);

	if ( r ) return FALSE;  /* IM ate it */

	if ( (p->editing_object != NULL)
	  && (p->editing_object->type == TEXT) )
	{
		switch ( event->keyval ) {

		case GDK_KEY_BackSpace :
			handle_text_backspace(p->editing_object);
			redraw_object(p->editing_object);
			break;

		case GDK_KEY_Left :
			move_cursor_left(p->editing_object);
			redraw_object(p->editing_object);
			break;

		case GDK_KEY_Right :
			move_cursor_right(p->editing_object);
			redraw_object(p->editing_object);
			break;

		}
	}

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

	}

	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                                 struct presentation *p)
{
	if ( p->editing_object != NULL ) {
		p->cur_tool->drag_object(p->cur_tool, p, p->editing_object,
		                         event->x - p->border_offs_x,
		                         event->y - p->border_offs_y);
	}

	gdk_event_request_motions(event);
	return FALSE;
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 struct presentation *p)
{
	struct object *clicked;
	gdouble x, y;

	x = event->x - p->border_offs_x;
	y = event->y - p->border_offs_y;

	clicked = find_object_at_position(p->view_slide, x, y);

	if ( clicked == NULL ) {

		if ( p->editing_object != NULL ) {
			p->cur_tool->deselect(p->editing_object, p->cur_tool);
			p->editing_object = NULL;
		}
		p->cur_tool->click_create(p, p->cur_tool, x, y);

	} else {

		p->editing_object = clicked;
		p->cur_tool->click_select(p, p->cur_tool, x, y);

	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw_overlay(p);
	return FALSE;
}


static void draw_editing_bits(cairo_t *cr, struct presentation *p,
                              struct object *o)
{
	p->cur_tool->draw_editing_overlay(cr, o);

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


static gboolean expose_sig(GtkWidget *da, GdkEventExpose *event,
                           struct presentation *p)
{
	cairo_t *cr;
	GtkAllocation allocation;
	double xoff, yoff;

	check_redraw_slide(p->view_slide);

	cr = gdk_cairo_create(da->window);

	/* Overall background */
	cairo_rectangle(cr, event->area.x, event->area.y,
	                event->area.width, event->area.height);
	if ( p->slideshow == NULL ) {
		cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
	} else {
		cairo_set_source_rgb(cr, 1.0, 0.3, 0.2);
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
	cairo_set_source_surface(cr, p->view_slide->render_cache, xoff, yoff);
	cairo_fill(cr);

	cairo_translate(cr, xoff, yoff);

	/* Draw editing bits for selected object */
	if ( p->editing_object != NULL ) {
		draw_editing_bits(cr, p, p->editing_object);
	}

	/* Draw dragging box if necessary */
	if ( p->draw_drag_box ) {
		draw_editing_box(cr, p->drag_x-(p->drag_width/2.0),
		                     p->drag_y-(p->drag_height/2.0),
		                     p->drag_width, p->drag_height);
	}

	cairo_destroy(cr);

	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, struct presentation *p)
{
	GdkAtom target;

	if ( !p->drag_preview_pending && !p->have_drag_data ) {
		target = gtk_drag_dest_find_target(widget, drag_context, NULL);
		gtk_drag_get_data(widget, drag_context, target, time);
		p->drag_preview_pending = 1;
	}

	if ( p->have_drag_data && !p->draw_drag_box ) {
		return FALSE;
	}

	gdk_drag_status(drag_context, GDK_ACTION_LINK, time);

	if ( p->draw_drag_box ) {

		/* FIXME: Honour margins */
		p->drag_x = x;
		p->drag_y = y;

		/* FIXME: Invalidate only the necessary region */
		gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

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


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, struct presentation *p)
{
	if ( p->drag_preview_pending ) {

		gchar *filename;
		GError *error = NULL;
		GdkPixbufFormat *f;
		const gchar *uri;

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
			return;

		}
		chomp(filename);

		f = gdk_pixbuf_get_file_info(filename, &p->drag_width,
		                                       &p->drag_height);
		g_free(filename);
		if ( f == NULL ) {

			gdk_drag_status(drag_context, 0, time);
			if ( p->drag_highlight ) {
				gtk_drag_unhighlight(widget);
				p->drag_highlight = 0;
			}

		} else {

			/* Looks like a sensible image */

			gdk_drag_status(drag_context, GDK_ACTION_LINK, time);

			if ( !p->drag_highlight ) {
				gtk_drag_highlight(widget);
				p->drag_highlight = 1;
			}

			/* Scale the image down if it's a silly size */
			if ( p->drag_width > p->slide_width ) {
				int new_drag_width;
				new_drag_width = p->slide_width/2;
				p->drag_height = (new_drag_width*p->drag_height)
				                  / p->drag_width;
				p->drag_width = new_drag_width;
			}

			if ( p->drag_height > p->slide_height ) {
				int new_drag_height;
				new_drag_height = p->slide_height/2;
				p->drag_width = (new_drag_height*p->drag_width)
				                 / p->drag_height;
				p->drag_height = new_drag_height;
			}

			p->draw_drag_box = 1;

		}

	} else {

		printf("Drop: '%s'\n", seldata->data);
		gtk_drag_finish(drag_context, TRUE, FALSE, time);

		/* FIXME: Create a new image object according to image size and
		 * current margins, consistent with box drawn for the preview */

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
	p->draw_drag_box = 0;
}


int open_mainwindow(struct presentation *p)
{
	GtkWidget *window;
	GtkWidget *vbox;
	char *title;
	GtkWidget *sw;
	GtkTargetEntry targets[1];

	if ( p->window != NULL ) {
		fprintf(stderr, "Presentation window is already open!\n");
		return 1;
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	p->window = window;

	title = malloc(strlen(p->titlebar)+14);
	sprintf(title, "%s - Colloquium", p->titlebar);
	gtk_window_set_title(GTK_WINDOW(window), title);
	free(title);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(close_sig), p);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	add_menu_bar(p, vbox);

	p->drawingarea = gtk_drawing_area_new();
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw),
	                                      p->drawingarea);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request(GTK_WIDGET(p->drawingarea),
	                            p->slide_width + 20,
	                            p->slide_height + 20);

	p->select_tool = initialise_select_tool();
	p->text_tool = initialise_text_tool(p->drawingarea);
	p->cur_tool = p->select_tool;

	gtk_widget_set_can_focus(GTK_WIDGET(p->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(p->drawingarea),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	g_signal_connect(G_OBJECT(p->drawingarea), "button-press-event",
			 G_CALLBACK(button_press_sig), p);
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
	check_redraw_slide(p->view_slide);

	gtk_widget_grab_focus(GTK_WIDGET(p->drawingarea));

	gtk_widget_show_all(window);
	return 0;
}
