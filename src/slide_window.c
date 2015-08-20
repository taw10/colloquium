/*
 * slide_window.c
 *
 * Copyright © 2013-2015 Thomas White <taw@bitwiz.org.uk>
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

	SCEditor            *sceditor;

	struct menu_pl      *style_menu;
	int                  n_style_menu;

	struct slide        *cur_slide;  /* FIXME: SPOT inside SCEditor */

	SlideShow           *show;
	struct notes        *notes;
};



static void update_toolbar(SlideWindow *sw)
{
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);
	if ( cur_slide_number == 0 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(sw->bfirst), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(sw->bprev), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(sw->bfirst), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(sw->bprev), TRUE);
	}

	if ( cur_slide_number == sw->p->num_slides-1 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(sw->bnext), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(sw->blast), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(sw->bnext), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(sw->blast), TRUE);
	}

}


/* Inelegance to make furniture selection menus work */
struct menu_pl
{
	SlideWindow *sw;
	char *style_name;
	GtkWidget *widget;
};


static gint UNUSED add_furniture(GtkWidget *widget, struct menu_pl *pl)
{
	sc_block_append_end(pl->sw->cur_slide->scblocks,
	                    strdup(pl->style_name), NULL, NULL);

	//do_slide_update(pl->p, pl->sw->pc); FIXME

	return 0;
}


static void UNUSED update_style_menus(SlideWindow *sw)
{
	//GtkWidget *menu;
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

#if 0   //  FIXME
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
#endif
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


static void saveas_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GtkWidget *d;
	SlideWindow *sw = vp;

	d = gtk_file_chooser_dialog_new("Save Presentation",
	                                GTK_WINDOW(sw->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                "_Save", GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(saveas_response_sig), sw);

	gtk_widget_show_all(d);
}


static void save_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	SlideWindow *sw = vp;

	if ( sw->p->filename == NULL ) {
		return saveas_sig(NULL, NULL, sw);
	}

	save_presentation(sw->p, sw->p->filename);
}


static void delete_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	SlideWindow *sw = vp;
	sc_editor_delete_selected_frame(sw->sceditor);
}


static void add_slide_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	SlideWindow *sw = vp;
	struct slide *new;
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);

	new = add_slide(sw->p, cur_slide_number+1);
	new->scblocks = sc_block_insert_after(sw->cur_slide->scblocks,
	                                      "slide", NULL, NULL);

	change_edit_slide(sw, new);

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


static void exportpdf_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	SlideWindow *sw = vp;
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Export PDF",
	                                GTK_WINDOW(sw->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                "_Export", GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(export_pdf_response_sig), sw);

	gtk_widget_show_all(d);
}


void slidewindow_notes_closed(SlideWindow *sw)
{
	sw->notes = NULL;
}


/* Change the editor's slide to "np" */
void change_edit_slide(SlideWindow *sw, struct slide *np)
{
	sw->cur_slide = np;

	update_toolbar(sw);

	sc_editor_set_slidenum(sw->sceditor, 1+slide_number(sw->p, np));
	sc_editor_set_scblock(sw->sceditor, np->scblocks);

	if ( sw->notes != NULL ) notes_set_slide(sw->notes, np);

	if ( slideshow_linked(sw->show) ) {
		change_proj_slide(sw->show, np);
	} /* else leave the slideshow alone */
}


static void change_slide_first(SlideWindow *sw)
{
	change_edit_slide(sw, sw->p->slides[0]);
}


static void change_slide_backwards(SlideWindow *sw)
{
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);
	if ( cur_slide_number == 0 ) return;

	change_edit_slide(sw, sw->p->slides[cur_slide_number-1]);
}


static void change_slide_forwards(SlideWindow *sw)
{
	int cur_slide_number;

	cur_slide_number = slide_number(sw->p, sw->cur_slide);
	if ( cur_slide_number == sw->p->num_slides-1 ) return;

	change_edit_slide(sw, sw->p->slides[cur_slide_number+1]);
}


static void change_slide_last(SlideWindow *sw)
{
	change_edit_slide(sw, sw->p->slides[sw->p->num_slides-1]);
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


static void open_notes_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	SlideWindow *sw = vp;
	sw->notes = open_notes(sw, sw->cur_slide);
}


static void open_clock_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	SlideWindow *sw = vp;
	open_clock(sw->p);
}


static void slidewindow_set_background(SlideWindow *sw)
{
	if ( (sw->show != NULL) && !slideshow_linked(sw->show) ) {
		sc_editor_set_background(sw->sceditor, 1.0, 0.3, 0.2);
	} else {
		sc_editor_set_background(sw->sceditor, 0.9, 0.9, 0.9);
	}
}


void slidewindow_redraw(SlideWindow *sw)
{
	slidewindow_set_background(sw);
	sc_editor_redraw(sw->sceditor);
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


static gboolean close_sig(GtkWidget *w, SlideWindow *sw)
{
	sw->p->slidewindow = NULL;
	return FALSE;
}


static void ss_end_show(SlideShow *ss, void *vp)
{
	SlideWindow *sw = vp;
	sw->show = NULL;
}


static void ss_next_slide(SlideShow *ss, void *vp)
{
	SlideWindow *sw = vp;
	change_slide_forwards(sw);
}


static void ss_prev_slide(SlideShow *ss, void *vp)
{
	SlideWindow *sw = vp;
	change_slide_backwards(sw);
}


static void ss_changed_link(SlideShow *ss, void *vp)
{
	SlideWindow *sw = vp;
	slidewindow_redraw(sw);
}


static struct slide *ss_cur_slide(SlideShow *ss, void *vp)
{
	SlideWindow *sw = vp;
	return sw->cur_slide;
}


static void start_slideshow_sig(GSimpleAction *action, GVariant *parameter,
                                gpointer vp)
{
	SlideWindow *sw = vp;
	struct sscontrolfuncs ssc;

	ssc.next_slide = ss_next_slide;
	ssc.prev_slide = ss_prev_slide;
	ssc.current_slide = ss_cur_slide;
	ssc.changed_link = ss_changed_link;
	ssc.end_show = ss_end_show;

	sw->show = try_start_slideshow(sw->p, ssc, sw);
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

		case GDK_KEY_B :
		case GDK_KEY_b :
		if ( sw->show != NULL ) {
			toggle_slideshow_link(sw->show);
		}
	}

	return FALSE;
}


GActionEntry sw_entries[] = {

	{ "save", save_sig, NULL, NULL, NULL },
	{ "saveas", saveas_sig, NULL, NULL, NULL },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },
	{ "deleteframe", delete_frame_sig, NULL, NULL, NULL },
	{ "slide", add_slide_sig, NULL, NULL, NULL },
	{ "startslideshow", start_slideshow_sig, NULL, NULL, NULL },
	{ "notes", open_notes_sig, NULL, NULL, NULL },
	{ "clock", open_clock_sig, NULL, NULL, NULL },
	{ "first", first_slide_sig, NULL, NULL, NULL },
	{ "prev", prev_slide_sig, NULL, NULL, NULL },
	{ "next", next_slide_sig, NULL, NULL, NULL },
	{ "last", last_slide_sig, NULL, NULL, NULL },
};


SlideWindow *slide_window_open(struct presentation *p, GApplication *app)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *scroll;
	GtkWidget *toolbar;
	GtkToolItem *button;
	SlideWindow *sw;
	SCBlock *stylesheets[2];
	GtkWidget *image;

	if ( p->slidewindow != NULL ) {
		fprintf(stderr, "Slide window is already open!\n");
		return p->slidewindow;
	}

	sw = calloc(1, sizeof(SlideWindow));
	if ( sw == NULL ) return NULL;

	window = gtk_application_window_new(GTK_APPLICATION(app));
	g_action_map_add_action_entries(G_ACTION_MAP(window), sw_entries,
	                                G_N_ELEMENTS(sw_entries), sw);
	sw->window = window;
	sw->p = p;
	sw->cur_slide = p->slides[0];
	sw->show = NULL;

	update_titlebar(p);

	g_signal_connect(G_OBJECT(window), "destroy",
	                 G_CALLBACK(close_sig), sw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(toolbar), FALSE, FALSE, 0);

	/* Fullscreen */
	image = gtk_image_new_from_icon_name("view-fullscreen",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, "Start slideshow");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.startslideshow");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	/* Add slide */
	image = gtk_image_new_from_icon_name("add",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, "Add slide");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	/* Change slide.  FIXME: LTR vs RTL */
	image = gtk_image_new_from_icon_name("gtk-goto-first-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	sw->bfirst = gtk_tool_button_new(image, "First slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(sw->bfirst));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(sw->bfirst),
	                               "win.first");

	image = gtk_image_new_from_icon_name("gtk-go-back-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	sw->bprev = gtk_tool_button_new(image, "Previous slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(sw->bprev));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(sw->bprev),
	                               "win.prev");

	image = gtk_image_new_from_icon_name("gtk-go-forward-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	sw->bnext = gtk_tool_button_new(image, "Next slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(sw->bnext));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(sw->bnext),
	                               "win.next");

	image = gtk_image_new_from_icon_name("gtk-goto-last-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	sw->blast = gtk_tool_button_new(image, "Last slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(sw->blast));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(sw->blast),
	                               "win.last");
	update_toolbar(sw);

	stylesheets[0] = p->stylesheet;
	stylesheets[1] = NULL;
	sw->sceditor = sc_editor_new(sw->cur_slide->scblocks, stylesheets);
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
	                                      GTK_WIDGET(sw->sceditor));
	gtk_window_set_focus(GTK_WINDOW(window), GTK_WIDGET(sw->sceditor));
	g_signal_connect(G_OBJECT(sw->sceditor), "key-press-event",
			 G_CALLBACK(key_press_sig), sw);

	/* Size of SCEditor surface in pixels */

	/* FIXME: Somewhat arbitrary.  Should come from slide itself */
	sc_editor_set_size(sw->sceditor, 1024, 768);
	sc_editor_set_logical_size(sw->sceditor, 1024.0, 768.0);

	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	/* Default size */
	gtk_window_set_default_size(GTK_WINDOW(sw->window), 1024+100, 768+150);
	gtk_window_set_resizable(GTK_WINDOW(sw->window), TRUE);

	/* Initial background colour */
	slidewindow_set_background(sw);

	gtk_widget_show_all(window);

	return sw;
}
