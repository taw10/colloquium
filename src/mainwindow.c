/*
 * mainwindow.c
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
#include "mainwindow.h"
#include "render.h"
#include "frame.h"
#include "slideshow.h"
#include "wrap.h"
#include "notes.h"
#include "pr_clock.h"
#include "slide_sorter.h"
#include "sc_parse.h"
#include "sc_interp.h"


/* Update a slide, once it's been edited in some way. */
void rerender_slide(struct presentation *p)
{
	struct slide *s = p->cur_edit_slide;
	int n = slide_number(p, s);

	free_render_buffers(s);

	s->rendered_thumb = render_slide(s, s->parent->thumb_slide_width,
	                                 p->slide_width, p->slide_height, p->is,
	                                 ISZ_THUMBNAIL, n);

	s->rendered_proj = render_slide(s, s->parent->proj_slide_width,
	                                p->slide_width, p->slide_height, p->is,
	                                ISZ_SLIDESHOW, n);

	s->rendered_edit = render_slide(s, s->parent->edit_slide_width,
	                                p->slide_width, p->slide_height, p->is,
	                                ISZ_EDITOR, n);
}


/* Ensure that "edit" and "proj" renderings are in order */
static void render_edit_and_proj(struct presentation *p)
{
	struct slide *s = p->cur_edit_slide;
	int n = slide_number(p, s);

	if ( s->rendered_proj == NULL ) {
		s->rendered_proj = render_slide(s, s->parent->proj_slide_width,
		                               p->slide_width, p->slide_height,
		                               p->is, ISZ_SLIDESHOW, n);
	}

	if ( s->rendered_edit == NULL ) {
		s->rendered_edit = render_slide(s, s->parent->edit_slide_width,
		                                p->slide_width, p->slide_height,
		                                p->is, ISZ_EDITOR, n);
	}
}



/* Force a redraw of the editor window */
void redraw_editor(struct presentation *p)
{
	gint w, h;

	w = gtk_widget_get_allocated_width(GTK_WIDGET(p->drawingarea));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(p->drawingarea));

	gtk_widget_queue_draw_area(p->drawingarea, 0, 0, w, h);
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


void update_toolbar(struct presentation *p)
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


static void do_slide_update(struct presentation *p, PangoContext *pc)
{
	rerender_slide(p);
	redraw_editor(p);
	if ( (p->slideshow != NULL)
	  && (p->cur_edit_slide == p->cur_proj_slide) )
	{
		redraw_slideshow(p);
	}
}


/* Inelegance to make furniture selection menus work */
struct menu_pl
{
	struct presentation *p;
	char *style_name;
	GtkWidget *widget;
};


static gint add_furniture(GtkWidget *widget, struct menu_pl *pl)
{
	sc_block_append_end(pl->p->cur_edit_slide->scblocks,
	                    strdup(pl->style_name), NULL, NULL);

	do_slide_update(pl->p, pl->p->pc);

	return 0;
}


static void update_style_menus(struct presentation *p)
{
	GtkWidget *menu;
	SCInterpreter *scin;
	struct style_id *styles;
	int i, n_sty;

	/* Free old list */
	for ( i=0; i<p->n_style_menu; i++ ) {
		gtk_widget_destroy(p->style_menu[i].widget);
		free(p->style_menu[i].style_name);
	}
	free(p->style_menu);

	/* Get the list of styles from the style sheet */
	scin = sc_interp_new(NULL, NULL);
	if ( scin == NULL ) {
		fprintf(stderr, "Failed to set up interpreter.\n");
		return;
	}
	sc_interp_run_stylesheet(scin, p->stylesheet);

	styles = list_styles(scin, &n_sty);
	if ( styles == NULL ) return;

	sc_interp_destroy(scin);

	/* Set up list for next time */
	p->style_menu = calloc(n_sty, sizeof(struct menu_pl));
	if ( p->style_menu == NULL ) return;

	/* Add the styles to the "Insert" menu */
	menu = gtk_ui_manager_get_widget(p->ui, "/displaywindow/insert");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu));

	for ( i=0; i<n_sty; i++ ) {

		GtkWidget *item;

		item = gtk_menu_item_new_with_label(styles[i].friendlyname);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		p->style_menu[i].p = p;
		p->style_menu[i].widget = item;
		p->style_menu[i].style_name = styles[i].name;

		g_signal_connect(G_OBJECT(item), "activate",
		                 G_CALLBACK(add_furniture),
		                 &p->style_menu[i]);

		free(styles[i].friendlyname);
	}

	gtk_widget_show_all(menu);
	free(styles);
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
			p->cur_edit_slide = p->slides[0];
			rerender_slide(p);
			update_toolbar(p);
			update_style_menus(p);
			if ( p->slideshow != NULL ) end_slideshow(p);

		} else {

			struct presentation *p;

			/* FIXME */
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


static gint loadstyle_response_sig(GtkWidget *d, gint response,
			      struct presentation *p)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
		/* FIXME: Implement this (now easy) */
		//replace_stylesheet(p, filename);
		g_free(filename);
		update_style_menus(p);
		rerender_slide(p);

	}

	gtk_widget_destroy(d);

	return 0;
}


static gint loadstyle_sig(GtkWidget *widget, struct presentation *p)
{
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Load Stylesheet",
	                                GTK_WINDOW(p->window),
	                                GTK_FILE_CHOOSER_ACTION_OPEN,
	                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
	                                NULL);

	g_signal_connect(G_OBJECT(d), "response",
			 G_CALLBACK(loadstyle_response_sig), p);

	gtk_widget_show_all(d);

	return 0;
}


static gint new_sig(GtkWidget *widget, struct presentation *pnn)
{
	struct presentation *p;

	p = new_presentation();
	if ( p != NULL ) {
		struct slide *new;
		new = add_slide(p, 0);
		p->completely_empty = 1;
		/* FIXME: position */
		new->scblocks = sc_block_append_end(p->scblocks, "slide",
	                                            NULL, NULL);
		attach_notes(new);
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

		/* FIXME: Implement this */
//		if ( save_stylesheet(p->ss, filename) ) {
//			show_error(p, "Failed to save style sheet");
//		}

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

	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(window),
	        "Colloquium");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window),
	        PACKAGE_VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(window),
		"© 2013 Thomas White <taw@bitwiz.org.uk>");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(window),
		"A tiny presentation program");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(window),
		"© 2013 Thomas White <taw@bitwiz.org.uk>\n");
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


/* Change the editor's slide to "np" */
void change_edit_slide(struct presentation *p, struct slide *np)
{
	/* If this slide is not being shown on the projector, we can free the
	 * buffers */
	if ( p->cur_proj_slide != p->cur_edit_slide ) {
		free_render_buffers_except_thumb(p->cur_edit_slide);
	}

	p->cur_edit_slide = np;
	render_edit_and_proj(p);

	set_selection(p, NULL);
	update_toolbar(p);
	redraw_editor(p);

	if ( p->notes != NULL ) {
		notify_notes_slide_changed(p, np);
	}

	if ( (p->slideshow != NULL) && p->slideshow_linked ) {
		change_proj_slide(p, np);
	} /* else leave the slideshow alone */
}


static gint add_slide_sig(GtkWidget *widget, struct presentation *p)
{
	struct slide *new;
	int cur_slide_number;

	cur_slide_number = slide_number(p, p->cur_edit_slide);

	new = add_slide(p, cur_slide_number+1);
	new->scblocks = sc_block_insert_after(p->cur_edit_slide->scblocks,
	                                      "slide", NULL, NULL);

	change_edit_slide(p, new);

	return FALSE;
}


static gint first_slide_sig(GtkWidget *widget, struct presentation *p)
{
	change_edit_slide(p, p->slides[0]);
	return FALSE;
}


static gint prev_slide_sig(GtkWidget *widget, struct presentation *p)
{
	int cur_slide_number;

	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == 0 ) return FALSE;

	change_edit_slide(p, p->slides[cur_slide_number-1]);

	return FALSE;
}


static gint next_slide_sig(GtkWidget *widget, struct presentation *p)
{
	int cur_slide_number;

	cur_slide_number = slide_number(p, p->cur_edit_slide);
	if ( cur_slide_number == p->num_slides-1 ) return FALSE;

	change_edit_slide(p, p->slides[cur_slide_number+1]);

	return FALSE;
}


static gint last_slide_sig(GtkWidget *widget, struct presentation *p)
{
	change_edit_slide(p, p->slides[p->num_slides-1]);

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
	open_notes(p);
	return FALSE;
}


static gint open_clock_sig(GtkWidget *widget, struct presentation *p)
{
	open_clock(p);
	return FALSE;
}


static gint open_slidesorter_sig(GtkWidget *widget, struct presentation *p)
{
	open_slidesorter(p);
	return FALSE;
}

static gint delete_frame_sig(GtkWidget *widget, struct presentation *p)
{
	int i;

	for ( i=0; i<p->n_selection; i++ ) {
		delete_subframe(p->cur_edit_slide, p->selection[i]);
	}
	p->n_selection = 0;

	rerender_slide(p);
	redraw_editor(p);

	return FALSE;
}


static void add_menu_bar(struct presentation *p, GtkWidget *vbox)
{
	GError *error = NULL;
	GtkWidget *toolbar;
	GtkWidget *menu;
	GtkWidget *item;

	GtkActionEntry entries[] = {

		{ "FileAction", NULL, "_File", NULL, NULL, NULL },
		{ "NewAction", GTK_STOCK_NEW, "_New",
			NULL, NULL, G_CALLBACK(new_sig) },
		{ "OpenAction", GTK_STOCK_OPEN, "_Open...",
			NULL, NULL, G_CALLBACK(open_sig) },
		{ "LoadStyleAction", NULL, "_Load Stylesheet...",
			NULL, NULL, G_CALLBACK(loadstyle_sig) },
		{ "SaveAction", GTK_STOCK_SAVE, "_Save",
			NULL, NULL, G_CALLBACK(save_sig) },
		{ "SaveAsAction", GTK_STOCK_SAVE_AS, "Save _As...",
			NULL, NULL, G_CALLBACK(saveas_sig) },
		{ "SaveStyleAction", NULL, "Save St_ylesheet",
			NULL, NULL, G_CALLBACK(save_ss_sig) },
		{ "ExportPDFAction", NULL, "Export PDF",
			NULL, NULL, G_CALLBACK(export_pdf_sig) },
		{ "QuitAction", GTK_STOCK_QUIT, "_Quit",
			NULL, NULL, G_CALLBACK(quit_sig) },

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
		{ "EditStyleAction", NULL, "Stylesheet...",
			NULL, NULL, G_CALLBACK(open_stylesheet_sig) },

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

	menu = gtk_ui_manager_get_widget(p->ui, "/displaywindow/insert");
	menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu));
	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	update_style_menus(p);
	update_toolbar(p);
}


static gint close_sig(GtkWidget *window, struct presentation *p)
{
	free_presentation(p);
	return 0;
}


static void draw_editing_box(cairo_t *cr, struct frame *fr)
{
	const double dash[] = {2.0, 2.0};
	double xmin, ymin, width, height;
	double ptot_w, ptot_h;

	xmin = fr->x;
	ymin = fr->y;
	width = fr->w;
	height = fr->h;

	cairo_new_path(cr);
	cairo_rectangle(cr, xmin, ymin, width, height);
	cairo_set_source_rgb(cr, 0.0, 0.69, 1.0);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);

	cairo_new_path(cr);
	ptot_w = fr->pad_l + fr->pad_r;
	ptot_h = fr->pad_t + fr->pad_b;
	cairo_rectangle(cr, xmin+fr->pad_l, ymin+fr->pad_t,
	                    width-ptot_w, height-ptot_h);
	cairo_set_dash(cr, dash, 2, 0.0);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.1);
	cairo_stroke(cr);

	cairo_set_dash(cr, NULL, 0, 0.0);
}


static void draw_caret(cairo_t *cr, struct frame *fr,
                       int cursor_line, int cursor_box, int cursor_pos)
{
	double xposd, yposd, line_height;
	double cx, clow, chigh;
	const double t = 1.8;
	struct wrap_box *box;
	int i;

	if ( fr == NULL ) return;
	if ( fr->n_lines == 0 ) return;

	/* Locate the cursor in a "logical" and "geographical" sense */
	box = &fr->lines[cursor_line].boxes[cursor_box];
	get_cursor_pos(box, cursor_pos, &xposd, &yposd, &line_height);
	xposd += fr->pad_l;
	yposd += fr->pad_t;

	for ( i=0; i<cursor_line; i++ ) {
		yposd += pango_units_to_double(fr->lines[i].height);
	}

	for ( i=0; i<cursor_box; i++ ) {
		int w = fr->lines[cursor_line].boxes[i].width;
		w += fr->lines[cursor_line].boxes[i].sp;
		xposd += pango_units_to_double(w);
	}

	cx = fr->x + xposd;
	clow = fr->y + yposd;
	chigh = clow + line_height;

	cairo_move_to(cr, cx, clow);
	cairo_line_to(cr, cx, chigh);

	cairo_move_to(cr, cx-t, clow-t);
	cairo_line_to(cr, cx, clow);
	cairo_move_to(cr, cx+t, clow-t);
	cairo_line_to(cr, cx, clow);

	cairo_move_to(cr, cx-t, chigh+t);
	cairo_line_to(cr, cx, chigh);
	cairo_move_to(cr, cx+t, chigh+t);
	cairo_line_to(cr, cx, chigh);

	cairo_set_source_rgb(cr, 0.86, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
}


static void draw_resize_handle(cairo_t *cr, double x, double y)
{
	cairo_new_path(cr);
	cairo_rectangle(cr, x, y, 20.0, 20.0);
	cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
	cairo_fill(cr);
}


static void draw_overlay(cairo_t *cr, struct presentation *p)
{
	int i;

	for ( i=0; i<p->n_selection; i++ ) {

		double x, y, w, h;

		draw_editing_box(cr, p->selection[i]);

		x = p->selection[i]->x;
		y = p->selection[i]->y;
		w = p->selection[i]->w;
		h = p->selection[i]->h;

		/* Draw resize handles */
		/* FIXME: Not if this frame can't be resized */
		draw_resize_handle(cr, x, y+h-20.0);
		draw_resize_handle(cr, x+w-20.0, y);
		draw_resize_handle(cr, x, y);
		draw_resize_handle(cr, x+w-20.0, y+h-20.0);
	}

	/* If only one frame is selected, draw the caret */
	if ( p->n_selection == 1 ) {
		draw_caret(cr, p->cursor_frame, p->cursor_line, p->cursor_box,
		               p->cursor_pos);
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

	if ( (p->drag_status == DRAG_STATUS_DRAGGING)
	  && ((p->drag_reason == DRAG_REASON_RESIZE)
	   || (p->drag_reason == DRAG_REASON_MOVE)) )
	{
		cairo_new_path(cr);
		cairo_rectangle(cr, p->box_x, p->box_y,
		                    p->box_width, p->box_height);
		cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
		cairo_set_line_width(cr, 0.5);
		cairo_stroke(cr);
	}
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr,
                         struct presentation *p)
{
	double xoff, yoff;
	int width, height;
	int edit_slide_height;

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
	edit_slide_height = (p->slide_height/p->slide_width)*p->edit_slide_width;
	xoff = (width - p->edit_slide_width)/2.0;
	yoff = (height - edit_slide_height)/2.0;
	p->border_offs_x = xoff;  p->border_offs_y = yoff;

	/* Draw the slide from the cache */
	if ( p->cur_edit_slide->rendered_edit != NULL ) {
		cairo_set_source_surface(cr, p->cur_edit_slide->rendered_edit,
			                 xoff, yoff);
		cairo_paint(cr);
	} else {
		fprintf(stderr, "Current slide not rendered yet!\n");
	}

	cairo_translate(cr, xoff, yoff);
	draw_overlay(cr, p);

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


static void fixup_cursor(struct presentation *p)
{
	struct frame *fr;
	struct wrap_line *sline;
	struct wrap_box *sbox;

	fr = p->cursor_frame;

	if ( p->cursor_line >= fr->n_lines ) {
		/* We find ourselves on a line which doesn't exist */
		p->cursor_line = fr->n_lines-1;
		p->cursor_box = fr->lines[fr->n_lines-1].n_boxes-1;
	}

	sline = &fr->lines[p->cursor_line];

	if ( p->cursor_box >= sline->n_boxes ) {

		/* We find ourselves in a box which doesn't exist */

		if ( p->cursor_line > fr->n_lines-1 ) {
			/* This isn't the last line, so go to the first box of
			 * the next line */
			p->cursor_line++;
			p->cursor_box = 0;
			sline = &p->cursor_frame->lines[p->cursor_line];
		} else {
			/* There are no more lines, so just go to the end */
			p->cursor_line = fr->n_lines-1;
			sline = &p->cursor_frame->lines[p->cursor_line];
			p->cursor_box = sline->n_boxes-1;
		}
	}

	sbox = &sline->boxes[p->cursor_box];

	if ( p->cursor_pos > sbox->len_chars ) {
		advance_cursor(p);
	}
}


static void move_cursor(struct presentation *p, signed int x, signed int y)
{
	if ( x > 0 ) {
		advance_cursor(p);
	} else {
		move_cursor_back(p);
	}
}


static void insert_text(char *t, struct presentation *p)
{
	int sln, sbx, sps;
	struct wrap_box *sbox;
	struct frame *fr = p->cursor_frame;

	if ( fr == NULL ) return;

	/* If this is, say, the top level frame, do nothing */
	if ( fr->boxes == NULL ) return;

	sln = p->cursor_line;
	sbx = p->cursor_box;
	sps = p->cursor_pos;
	sbox = &p->cursor_frame->lines[sln].boxes[sbx];

	sc_insert_text(sbox->scblock, sps+sbox->offs_char, t);

	fr->empty = 0;

	rerender_slide(p);

	fixup_cursor(p);
	advance_cursor(p);

	redraw_editor(p);
}


static void do_backspace(struct frame *fr, struct presentation *p)
{
	int sln, sbx, sps;

	if ( fr == NULL ) return;

	/* If this is, say, the top level frame, do nothing */
	if ( fr->n_lines == 0 ) return;

	sln = p->cursor_line;
	sbx = p->cursor_box;
	sps = p->cursor_pos;
	struct wrap_box *sbox = &p->cursor_frame->lines[sln].boxes[sbx];

	move_cursor_back(p);

	/* Delete may cross wrap boxes and maybe SCBlock boundaries */
	struct wrap_line *fline = &p->cursor_frame->lines[p->cursor_line];
	struct wrap_box *fbox = &fline->boxes[p->cursor_box];

//	SCBlock *scbl = sbox->scblock;
//	do {
//		show_sc_blocks(scbl);
//		scbl = sc_block_next(scbl);
//	} while ( (scbl != fbox->scblock) && (scbl != NULL) );

	if ( (fbox->scblock == NULL) || (sbox->scblock == NULL) ) return;
	sc_delete_text(fbox->scblock, p->cursor_pos+fbox->offs_char,
	               sbox->scblock, sps+sbox->offs_char);

//	scbl = sbox->scblock;
//	do {
//		show_sc_blocks(scbl);
//		scbl = sc_block_next(scbl);
//	} while ( (scbl != fbox->scblock) && (scbl != NULL) );

	rerender_slide(p);
	fixup_cursor(p);
	redraw_editor(p);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              struct presentation *p)
{
	if ( p->n_selection == 0 ) {
		if ( str[0] == 'b' ) {
			check_toggle_blank(p);
		} else {
			printf("IM keypress: %s\n", str);
		}
		return FALSE;
	}

	insert_text(str, p);

	return FALSE;
}


static int within_frame(struct frame *fr, double x, double y)
{
	if ( x < fr->x ) return 0;
	if ( y < fr->y ) return 0;
	if ( x > fr->x + fr->w ) return 0;
	if ( y > fr->y + fr->h ) return 0;
	return 1;
}


static struct frame *find_frame_at_position(struct frame *fr,
                                            double x, double y)
{
	int i;

	for ( i=0; i<fr->num_children; i++ ) {

		if ( within_frame(fr->children[i], x, y) ) {
			return find_frame_at_position(fr->children[i], x, y);
		}

	}

	if ( within_frame(fr, x, y) ) return fr;
	return NULL;
}


static enum corner which_corner(double xp, double yp, struct frame *fr)
{
	double x, y;  /* Relative to object position */

	x = xp - fr->x;
	y = yp - fr->y;

	if ( x < 0.0 ) return CORNER_NONE;
	if ( y < 0.0 ) return CORNER_NONE;
	if ( x > fr->w ) return CORNER_NONE;
	if ( y > fr->h ) return CORNER_NONE;

	/* Top left? */
	if ( (x<20.0) && (y<20.0) ) return CORNER_TL;
	if ( (x>fr->w-20.0) && (y<20.0) ) return CORNER_TR;
	if ( (x<20.0) && (y>fr->h-20.0) ) {
		return CORNER_BL;
	}
	if ( (x>fr->w-20.0) && (y>fr->h-20.0) ) {
		return CORNER_BR;
	}

	return CORNER_NONE;
}


static void calculate_box_size(struct frame *fr, struct presentation *p,
                               double x, double y)
{
	double ddx, ddy, dlen, mult;
	double vx, vy, dbx, dby;

	ddx = x - p->start_corner_x;
	ddy = y - p->start_corner_y;

	if ( !fr->is_image ) {

		switch ( p->drag_corner ) {

			case CORNER_BR :
			p->box_x = fr->x;
			p->box_y = fr->y;
			p->box_width = fr->w + ddx;
			p->box_height = fr->h + ddy;
			break;

			case CORNER_BL :
			p->box_x = fr->x + ddx;
			p->box_y = fr->y;
			p->box_width = fr->w - ddx;
			p->box_height = fr->h + ddy;
			break;

			case CORNER_TL :
			p->box_x = fr->x + ddx;
			p->box_y = fr->y + ddy;
			p->box_width = fr->w - ddx;
			p->box_height = fr->h - ddy;
			break;

			case CORNER_TR :
			p->box_x = fr->x;
			p->box_y = fr->y + ddy;
			p->box_width = fr->w + ddx;
			p->box_height = fr->h - ddy;
			break;

			case CORNER_NONE :
			break;

		}
		return;


	}

	switch ( p->drag_corner ) {

		case CORNER_BR :
		vx = fr->w;
		vy = fr->h;
		break;

		case CORNER_BL :
		vx = -fr->w;
		vy = fr->h;
		break;

		case CORNER_TL :
		vx = -fr->w;
		vy = -fr->h;
		break;

		case CORNER_TR :
		vx = fr->w;
		vy = -fr->h;
		break;

		case CORNER_NONE :
		default:
		vx = 0.0;
		vy = 0.0;
		break;

	}

	dlen = (ddx*vx + ddy*vy) / p->diagonal_length;
	mult = (dlen+p->diagonal_length) / p->diagonal_length;

	p->box_width = fr->w * mult;
	p->box_height = fr->h * mult;
	dbx = p->box_width - fr->w;
	dby = p->box_height - fr->h;

	if ( p->box_width < 40.0 ) {
		mult = 40.0 / fr->w;
	}
	if ( p->box_height < 40.0 ) {
		mult = 40.0 / fr->h;
	}
	p->box_width = fr->w * mult;
	p->box_height = fr->h * mult;
	dbx = p->box_width - fr->w;
	dby = p->box_height - fr->h;

	switch ( p->drag_corner ) {

		case CORNER_BR :
		p->box_x = fr->x;
		p->box_y = fr->y;
		break;

		case CORNER_BL :
		p->box_x = fr->x - dbx;
		p->box_y = fr->y;
		break;

		case CORNER_TL :
		p->box_x = fr->x - dbx;
		p->box_y = fr->y - dby;
		break;

		case CORNER_TR :
		p->box_x = fr->x;
		p->box_y = fr->y - dby;
		break;

		case CORNER_NONE :
		break;

	}

}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 struct presentation *p)
{
	enum corner c;
	gdouble x, y;
	struct frame *clicked;

	x = event->x - p->border_offs_x;
	y = event->y - p->border_offs_y;

	if ( (p->n_selection > 0) && within_frame(p->selection[0], x, y) ) {
		clicked = p->selection[0];
	} else {
		clicked = find_frame_at_position(p->cur_edit_slide->top, x, y);
	}

	/* If the user clicked the currently selected frame, position cursor
	 * or possibly prepare for resize */
	if ( (p->n_selection > 0) && (clicked == p->selection[0]) ) {

		struct frame *fr;

		fr = p->selection[0];

		/* Within the resizing region? */
		c = which_corner(x, y, fr);
		if ( c != CORNER_NONE ) {

			p->drag_reason = DRAG_REASON_RESIZE;
			p->drag_corner = c;

			p->start_corner_x = x;
			p->start_corner_y = y;
			p->diagonal_length = pow(fr->w, 2.0);
			p->diagonal_length += pow(fr->h, 2.0);
			p->diagonal_length = sqrt(p->diagonal_length);

			calculate_box_size(fr, p, x, y);

			p->drag_status = DRAG_STATUS_COULD_DRAG;
			p->drag_reason = DRAG_REASON_RESIZE;

		} else {

			p->cursor_frame = clicked;
			find_cursor(clicked, x-fr->x, y-fr->y,
			            &p->cursor_line, &p->cursor_box,
			            &p->cursor_pos);

			p->start_corner_x = event->x - p->border_offs_x;
			p->start_corner_y = event->y - p->border_offs_y;
			p->drag_status = DRAG_STATUS_COULD_DRAG;
			p->drag_reason = DRAG_REASON_MOVE;

		}

	} else if ( (clicked == NULL) || (clicked == p->cur_edit_slide->top) ) {

		/* Clicked no object. Deselect old object and set up for
		 * (maybe) creating a new one. */
		set_selection(p, NULL);
		p->start_corner_x = event->x - p->border_offs_x;
		p->start_corner_y = event->y - p->border_offs_y;
		p->drag_status = DRAG_STATUS_COULD_DRAG;
		p->drag_reason = DRAG_REASON_CREATE;

	} else {

		/* Select new frame, no immediate dragging */
		p->drag_status = DRAG_STATUS_NONE;
		p->drag_reason = DRAG_REASON_NONE;
		set_selection(p, clicked);

	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw_editor(p);
	return FALSE;
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                           struct presentation *p)
{
	struct frame *fr = p->selection[0];
	gdouble x, y;

	x = event->x - p->border_offs_x;
	y = event->y - p->border_offs_y;

	if ( p->drag_status == DRAG_STATUS_COULD_DRAG ) {

		/* We just got a motion signal, and the status was "could drag",
		 * therefore the drag has started. */
		p->drag_status = DRAG_STATUS_DRAGGING;

	}

	switch ( p->drag_reason ) {

		case DRAG_REASON_NONE :
		break;

		case DRAG_REASON_CREATE :
		p->drag_corner_x = x;
		p->drag_corner_y = y;
		redraw_editor(p);
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled by dnd_motion() */
		break;

		case DRAG_REASON_RESIZE :
		calculate_box_size(fr, p,  x, y);
		redraw_editor(p);
		break;

		case DRAG_REASON_MOVE :
		p->box_x = (fr->x - p->start_corner_x) + x;
		p->box_y = (fr->y - p->start_corner_y) + y;
		p->box_width = fr->w;
		p->box_height = fr->h;
		redraw_editor(p);
		break;

	}

	gdk_event_request_motions(event);
	return FALSE;
}


static struct frame *create_frame(struct presentation *p, double x, double y,
                                  double w, double h)
{
	struct frame *parent;
	struct frame *fr;

	parent = p->cur_edit_slide->top;

	if ( w < 0.0 ) {
		x += w;
		w = -w;
	}

	if ( h < 0.0 ) {
		y += h;
		h = -h;
	}

	fr = add_subframe(parent);

	/* Add to SC */
	fr->scblocks = sc_block_append_end(p->cur_edit_slide->scblocks,
	                                   "f", NULL, NULL);
	sc_block_set_frame(fr->scblocks, fr);
	sc_block_append_inside(fr->scblocks, NULL, NULL, strdup(""));

	fr->x = x;
	fr->y = y;
	fr->w = w;
	fr->h = h;
	fr->is_image = 0;
	fr->empty = 1;

	update_geom(fr);

	return fr;
}


static void do_resize(struct presentation *p, double x, double y,
                                              double w, double h)
{
	struct frame *fr;

	assert(p->n_selection > 0);

	if ( w < 0.0 ) {
		w = -w;
		x -= w;
	}

	if ( h < 0.0 ) {
		h = -h;
		y -= h;
	}

	fr = p->selection[0];
	fr->x = x;
	fr->y = y;
	fr->w = w;
	fr->h = h;
	update_geom(fr);

	rerender_slide(p);
	redraw_editor(p);
}


static gboolean button_release_sig(GtkWidget *da, GdkEventButton *event,
                                   struct presentation *p)
{
	gdouble x, y;
	struct frame *fr;

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
		fr = create_frame(p, p->start_corner_x, p->start_corner_y,
		                     p->drag_corner_x - p->start_corner_x,
		                     p->drag_corner_y - p->start_corner_y);
		rerender_slide(p);
		set_selection(p, fr);
		break;

		case DRAG_REASON_IMPORT :
		/* Do nothing, handled in dnd_drop() or dnd_leave() */
		break;

		case DRAG_REASON_RESIZE :
		do_resize(p, p->box_x, p->box_y, p->box_width, p->box_height);
		break;

		case DRAG_REASON_MOVE :
		do_resize(p, p->box_x, p->box_y, p->box_width, p->box_height);
		break;

	}

	p->drag_reason = DRAG_REASON_NONE;

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw_editor(p);
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              struct presentation *p)
{
	gboolean r;
	int claim = 0;

	/* Throw the event to the IM context and let it sort things out */
	r = gtk_im_context_filter_keypress(GTK_IM_CONTEXT(p->im_context),
		                           event);
	if ( r ) return FALSE;  /* IM ate it */

	switch ( event->keyval ) {

		case GDK_KEY_Page_Up :
		prev_slide_sig(NULL, p);
		claim = 1;
		break;

		case GDK_KEY_Page_Down :
		next_slide_sig(NULL, p);
		claim = 1;
		break;

		case GDK_KEY_Escape :
		if ( p->slideshow != NULL ) end_slideshow(p);
		set_selection(p, NULL);
		redraw_editor(p);
		claim = 1;
		break;

		case GDK_KEY_Left :
		if ( p->n_selection == 1 ) {
			move_cursor(p, -1, 0);
			redraw_editor(p);
		}
		claim = 1;
		break;

		case GDK_KEY_Right :
		if ( p->n_selection == 1 ) {
			move_cursor(p, +1, 0);
			redraw_editor(p);
		}
		claim = 1;
		break;

		case GDK_KEY_Up :
		if ( p->n_selection == 1 ) {
			move_cursor(p, 0, -1);
			redraw_editor(p);
		}
		claim = 1;
		break;

		case GDK_KEY_Down :
		if ( p->n_selection == 1 ) {
			move_cursor(p, 0, +1);
			redraw_editor(p);
		}
		claim = 1;
		break;


		case GDK_KEY_Return :
		im_commit_sig(NULL, "\n", p);
		claim = 1;
		break;

		case GDK_KEY_BackSpace :
		if ( p->n_selection == 1 ) {
			do_backspace(p->selection[0], p);
			claim = 1;
		}
		break;

		case GDK_KEY_B :
		case GDK_KEY_b :
		if ( p->slideshow != NULL ) {
			//if ( p->prefs->b_splits ) {
				toggle_slideshow_link(p);
			//} else {
			//	p->ss_blank = 1-p->ss_blank;
			//	redraw_slideshow(p);
			//}
		}
		claim = 1;
		break;

	}

	if ( claim ) return TRUE;
	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, struct presentation *p)
{
	GdkAtom target;

	/* If we haven't already requested the data, do so now */
	if ( !p->drag_preview_pending && !p->have_drag_data ) {

		target = gtk_drag_dest_find_target(widget, drag_context, NULL);

		if ( target != GDK_NONE ) {
			gtk_drag_get_data(widget, drag_context, target, time);
			p->drag_preview_pending = 1;
		} else {
			p->import_acceptable = 0;
			gdk_drag_status(drag_context, 0, time);
		}

	}

	if ( p->have_drag_data && p->import_acceptable ) {

		gdk_drag_status(drag_context, GDK_ACTION_LINK, time);
		p->start_corner_x = x - p->import_width/2.0;
		p->start_corner_y = y - p->import_height/2.0;
		p->drag_corner_x = x + p->import_width/2.0;
		p->drag_corner_y = y + p->import_height/2.0;

		redraw_editor(p);

	}

	return TRUE;
}


static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                         gint x, gint y, guint time, struct presentation *p)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target(widget, drag_context, NULL);

	if ( target == GDK_NONE ) {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
	} else {
		gtk_drag_get_data(widget, drag_context, target, time);
	}

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

		gchar *filename = NULL;
		GdkPixbufFormat *f;
		gchar **uris;
		int w, h;

		p->have_drag_data = 1;
		p->drag_preview_pending = 0;
		uris = gtk_selection_data_get_uris(seldata);
		if ( uris != NULL ) {
			filename = g_filename_from_uri(uris[0], NULL, NULL);
		}
		g_strfreev(uris);

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
			gdk_drag_status(drag_context, GDK_ACTION_PRIVATE, time);
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

		gchar **uris;
		char *filename = NULL;

		uris = gtk_selection_data_get_uris(seldata);
		if ( uris != NULL ) {
			filename = g_filename_from_uri(uris[0], NULL, NULL);
		}
		g_strfreev(uris);

		if ( filename != NULL ) {

			struct frame *fr;
			char *opts;
			size_t len;
			int w, h;

			gtk_drag_finish(drag_context, TRUE, FALSE, time);
			chomp(filename);

			w = p->drag_corner_x - p->start_corner_x;
			h = p->drag_corner_y - p->start_corner_y;

			len = strlen(filename)+64;
			opts = malloc(len);
			if ( opts == NULL ) {
				free(filename);
				fprintf(stderr, "Failed to allocate SC\n");
				return;
			}
			snprintf(opts, len, "1fx1f+0+0,filename=\"%s\"",
			         filename);

			fr = create_frame(p, p->start_corner_x,
			                     p->start_corner_y, w, h);
			fr->is_image = 1;
			fr->empty = 0;
			sc_block_append_inside(fr->scblocks, "image", opts, "");
			show_hierarchy(p->cur_edit_slide->top, "");
			rerender_slide(p);
			set_selection(p, fr);
			redraw_editor(p);
			free(filename);

		} else {

			gtk_drag_finish(drag_context, FALSE, FALSE, time);

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


static gint realise_sig(GtkWidget *da, struct presentation *p)
{
	GdkWindow *win;

	/* Keyboard and input method stuff */
	p->im_context = gtk_im_multicontext_new();
	win = gtk_widget_get_window(p->drawingarea);
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(p->im_context), win);
	gdk_window_set_accept_focus(win, TRUE);
	g_signal_connect(G_OBJECT(p->im_context), "commit",
			 G_CALLBACK(im_commit_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "key-press-event",
			 G_CALLBACK(key_press_sig), p);

	/* FIXME: Can do this "properly" by setting up a separate font map */
	p->pc = gtk_widget_get_pango_context(da);
	rerender_slide(p);

	return FALSE;
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
	g_signal_connect(G_OBJECT(p->drawingarea), "realize",
	                 G_CALLBACK(realise_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "button-press-event",
	                 G_CALLBACK(button_press_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "button-release-event",
	                 G_CALLBACK(button_release_sig), p);
	g_signal_connect(G_OBJECT(p->drawingarea), "motion-notify-event",
	                 G_CALLBACK(motion_sig), p);

	/* Drag and drop */
	targets[0].target = "text/uri-list";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(p->drawingarea, 0, targets, 1, GDK_ACTION_PRIVATE);
	g_signal_connect(p->drawingarea, "drag-data-received",
	                 G_CALLBACK(dnd_receive), p);
	g_signal_connect(p->drawingarea, "drag-motion",
	                 G_CALLBACK(dnd_motion), p);
	g_signal_connect(p->drawingarea, "drag-drop",
	                 G_CALLBACK(dnd_drop), p);
	g_signal_connect(p->drawingarea, "drag-leave",
	                 G_CALLBACK(dnd_leave), p);

	gtk_widget_set_can_focus(GTK_WIDGET(p->drawingarea), TRUE);
	gtk_widget_add_events(GTK_WIDGET(p->drawingarea),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	g_signal_connect(G_OBJECT(p->drawingarea), "draw",
			 G_CALLBACK(draw_sig), p);

	/* Default size */
	gtk_window_set_default_size(GTK_WINDOW(p->window), 1024+100, 768+150);
	gtk_window_set_resizable(GTK_WINDOW(p->window), TRUE);

	assert(p->num_slides > 0);

	gtk_widget_grab_focus(GTK_WIDGET(p->drawingarea));

	gtk_widget_show_all(window);

	return 0;
}
