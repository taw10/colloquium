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
	gtk_main_quit();
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


static gint set_tool_sig(GtkWidget *widget, GtkRadioAction *action,
                         struct presentation *p)
{
	switch ( gtk_radio_action_get_current_value(action) )
	{
		case 0 : p->tool = TOOL_SELECT; break;
		case 1 : p->tool = TOOL_TEXT; break;
	}
	p->editing_object = NULL;
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	return 0;
}


static void layout_changed_sig(GtkComboBox *combo, struct presentation *p)
{
	int n;

	if ( p->editing_object != NULL ) {
		printf("Can't change layout element!\n");
		return;
	}

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	p->cur_layout = p->ss->layout_elements[n];


}


static void text_style_changed_sig(GtkComboBox *combo, struct presentation *p)
{
	int n;

	n = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	p->cur_style = p->ss->text_styles[n];

	if ( p->editing_object != NULL ) {
		set_text_style(p->editing_object, p->cur_style);
		gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);
	}
}


static void add_menu_bar(struct presentation *p, GtkWidget *vbox)
{
	GError *error = NULL;
	GtkWidget *label;
	GtkWidget *combo;
	GtkToolItem *titem;
	GtkWidget *box;
	int i;
	GtkActionEntry entries[] = {

		{ "FileAction", NULL, "_File", NULL, NULL, NULL },
		{ "NewAction", GTK_STOCK_NEW, "_New",
			NULL, NULL, NULL },
		{ "OpenAction", GTK_STOCK_OPEN, "_Open...",
			NULL, NULL, NULL },
		{ "SaveAction", GTK_STOCK_SAVE, "_Save",
			NULL, NULL, NULL },
		{ "SaveAsAction", GTK_STOCK_SAVE_AS, "Save _As...",
			NULL, NULL, NULL },
		{ "SaveStyleAction", GTK_STOCK_SAVE_AS, "Save St_ylesheet",
			NULL, NULL, NULL },
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

	p->toolbar = gtk_ui_manager_get_widget(p->ui,
	                                       "/ui/displaywindowtoolbar");

	titem = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(p->toolbar), titem, -1);

	box = gtk_vbox_new(FALSE, 0.0);
	p->tbox = gtk_hbox_new(FALSE, 0.0);
	titem = gtk_tool_item_new();
	gtk_box_pack_start(GTK_BOX(box), p->tbox, FALSE, FALSE, 5.0);
	gtk_container_add(GTK_CONTAINER(titem), box);
	gtk_toolbar_insert(GTK_TOOLBAR(p->toolbar), titem, -1);
	label = gtk_label_new("Layout element:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(p->tbox), label, FALSE, FALSE, 5.0);
	combo = gtk_combo_box_new_text();
	for ( i=0; i<p->ss->n_layout_elements; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          p->ss->layout_elements[i]->name);
	}
	gtk_box_pack_start(GTK_BOX(p->tbox), combo, FALSE, FALSE, 5.0);
	g_signal_connect(G_OBJECT(combo), "changed",
	                 G_CALLBACK(layout_changed_sig), p);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

	label = gtk_label_new("Text style:");
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(p->tbox), label, FALSE, FALSE, 5.0);
	combo = gtk_combo_box_new_text();
	for ( i=0; i<p->ss->n_text_styles; i++ ) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo),
		                          p->ss->text_styles[i]->name);
	}
	gtk_box_pack_start(GTK_BOX(p->tbox), combo, FALSE, FALSE, 5.0);
	g_signal_connect(G_OBJECT(combo), "changed",
	                 G_CALLBACK(text_style_changed_sig), p);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

	update_toolbar(p);
}


static gint close_sig(GtkWidget *window, struct presentation *p)
{
	gtk_main_quit();
	return 0;
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              struct presentation *p)
{
	if ( p->editing_object == NULL ) return FALSE;
	if ( p->editing_object->type != TEXT ) return FALSE;

	insert_text(p->editing_object, str);

	/* FIXME: Invalidate only the necessary region */
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

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
			break;

		case GDK_KEY_Left :
			move_cursor_left(p->editing_object);
			break;

		case GDK_KEY_Right :
			move_cursor_right(p->editing_object);
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
		p->editing_object = NULL;
		break;

	}

	/* FIXME: Invalidate only the necessary region */
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                                 struct presentation *p)
{
	if ( p->tool == TOOL_SELECT ) {

		if ( p->editing_object != NULL ) {

			gdouble x, y;

			x = event->x - p->border_offs_x;
			y = event->y - p->border_offs_y;

			p->editing_object->x = x + p->drag_offs_x;
			p->editing_object->y = y + p->drag_offs_y;
			p->view_slide->object_seq++;

			/* FIXME: Invalidate only the necessary region */
			gdk_window_invalidate_rect(p->drawingarea->window,
			                           NULL, FALSE);


		}

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

	if ( p->editing_object && p->editing_object->empty ) {
		delete_object(p->editing_object);
	}
	p->editing_object = NULL;

	if ( (x>0.0) && (x<p->view_slide->slide_width)
	  && (y>0.0) && (y<p->view_slide->slide_height) )
	{
		clicked = find_object_at_position(p->view_slide, x, y);
		switch ( p->tool ) {
		case TOOL_SELECT :
			if ( clicked ) {
				p->editing_object = clicked;
				p->drag_offs_x = clicked->x - x;
				p->drag_offs_y = clicked->y - y;
			}
			break;
		case TOOL_TEXT :
			if ( !clicked ) {
				struct object *n;
				n = add_text_object(p->view_slide, x, y,
				                    p->cur_layout,
				                    p->cur_style);
				p->editing_object = n;
			} else {
				p->editing_object = clicked;
				position_caret(clicked, x, y);
			}
			break;
		}
	}

	gtk_widget_grab_focus(GTK_WIDGET(da));

	/* FIXME: Invalidate only the necessary region */
	gdk_window_invalidate_rect(p->drawingarea->window, NULL, FALSE);

	return FALSE;
}


static void draw_editing_box(cairo_t *cr, double xmin, double ymin,
                                          double width, double height)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, xmin-5.0, ymin-5.0, width+10.0, height+10.0);
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);
}


static void draw_editing_bits(cairo_t *cr, struct presentation *p,
                              struct object *o)
{
	draw_editing_box(cr, o->x, o->y, o->bb_width, o->bb_height);

	switch ( o->type ) {

	case TEXT :

		if ( p->tool == TOOL_TEXT ) draw_caret(cr, o);
		break;

	}
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

	cairo_destroy(cr);

	return FALSE;
}


int open_mainwindow(struct presentation *p)
{
	GtkWidget *window;
	GtkWidget *vbox;
	char *title;
	GtkWidget *sw;

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

	p->im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(p->im_context),
	                                 p->drawingarea->window);
	g_signal_connect(G_OBJECT(p->im_context), "commit",
			 G_CALLBACK(im_commit_sig), p);

	gtk_window_set_default_size(GTK_WINDOW(p->window), 1024+100, 768+100);
	gtk_window_set_resizable(GTK_WINDOW(p->window), TRUE);

	assert(p->num_slides > 0);
	check_redraw_slide(p->view_slide);

	gtk_widget_grab_focus(GTK_WIDGET(p->drawingarea));

	gtk_widget_show_all(window);
	return 0;
}
