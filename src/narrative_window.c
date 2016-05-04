/*
 * narrative_window.c
 *
 * Copyright © 2014-2015 Thomas White <taw@bitwiz.org.uk>
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
#include "sc_parse.h"
#include "render.h"
#include "testcard.h"
#include "pr_clock.h"
#include "print.h"


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
	SlideShow           *show;
	SCBlock             *sel_slide;
};


static void update_toolbar(NarrativeWindow *nw)
{
	int cur_slide_number;

	cur_slide_number = slide_number(nw->p, nw->sel_slide);
	if ( cur_slide_number == 0 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), TRUE);
	}

	if ( cur_slide_number == num_slides(nw->p)-1 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), TRUE);
	}
}


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


static void open_slidesorter_sig(GSimpleAction *action, GVariant *parameter,
                                 gpointer vp)
{
}


static void delete_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
}


static void add_slide_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	SCBlock *nsblock;
	NarrativeWindow *nw = vp;

	/* Split the current paragraph */
	nsblock = split_paragraph_at_cursor(nw->sceditor);

	/* Link the new SCBlock in */
	if ( nsblock != NULL ) {
		sc_block_append(nsblock, "slide", NULL, NULL, NULL);
	} else {
		fprintf(stderr, "Failed to split paragraph\n");
	}

	sc_editor_set_scblock(nw->sceditor,
	                      sc_editor_get_scblock(nw->sceditor));
}


static void ss_end_show(SlideShow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	nw->show = NULL;
}


static void ss_next_slide(SlideShow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	SCBlock *tt;

	tt = next_slide(nw->p, nw->sel_slide);
	if ( tt == NULL ) return;  /* Already on last slide */
	nw->sel_slide = tt;
	if ( slideshow_linked(nw->show) ) {
		change_proj_slide(nw->show, nw->sel_slide);
	} /* else leave the slideshow alone */
	update_toolbar(nw);
}


static void ss_prev_slide(SlideShow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	SCBlock *tt;

	tt = prev_slide(nw->p, nw->sel_slide);
	if ( tt == NULL ) return;  /* Already on first slide */
	nw->sel_slide = tt;
	if ( slideshow_linked(nw->show) ) {
		change_proj_slide(nw->show, nw->sel_slide);
	} /* else leave the slideshow alone */
	update_toolbar(nw);
}


static void first_slide_sig(GSimpleAction *action, GVariant *parameter,
                            gpointer vp)
{
	NarrativeWindow *nw = vp;
	SCBlock *tt;

	tt = first_slide(nw->p);
	if ( tt == NULL ) return;  /* Fail */
	nw->sel_slide = tt;
	if ( slideshow_linked(nw->show) ) {
		change_proj_slide(nw->show, nw->sel_slide);
	} /* else leave the slideshow alone */
	update_toolbar(nw);
}


static void prev_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	ss_prev_slide(NULL, vp);
}


static void next_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	ss_next_slide(NULL, vp);
}


static void last_slide_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	NarrativeWindow *nw = vp;
	SCBlock *tt;

	tt = last_slide(nw->p);
	if ( tt == NULL ) return;  /* Fail */
	nw->sel_slide = tt;
	if ( slideshow_linked(nw->show) ) {
		change_proj_slide(nw->show, nw->sel_slide);
	} /* else leave the slideshow alone */
	update_toolbar(nw);
}


static void ss_changed_link(SlideShow *ss, void *vp)
{
}


static SCBlock *ss_cur_slide(SlideShow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	return nw->sel_slide;
}


static void start_slideshow_sig(GSimpleAction *action, GVariant *parameter,
                                gpointer vp)
{
	NarrativeWindow *nw = vp;
	struct sscontrolfuncs ssc;

	if ( num_slides(nw->p) == 0 ) return;

	ssc.next_slide = ss_next_slide;
	ssc.prev_slide = ss_prev_slide;
	ssc.current_slide = ss_cur_slide;
	ssc.changed_link = ss_changed_link;
	ssc.end_show = ss_end_show;

	nw->sel_slide = first_slide(nw->p);

	nw->show = try_start_slideshow(nw->p, ssc, nw);
}


static void open_notes_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
}


static void open_clock_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	open_clock(nw->p);
}


static void testcard_sig(GSimpleAction *action, GVariant *parameter,
                         gpointer vp)
{
	NarrativeWindow *nw = vp;
	show_testcard(nw->p);
}


static gint export_pdf_response_sig(GtkWidget *d, gint response,
                                    struct presentation *p)
{
       if ( response == GTK_RESPONSE_ACCEPT ) {
               char *filename;
               filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
               export_pdf(p, filename);
               g_free(filename);
       }

       gtk_widget_destroy(d);

       return 0;
}


static void exportpdf_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
       struct presentation *p = vp;
       GtkWidget *d;

       d = gtk_file_chooser_dialog_new("Export PDF",
                                       NULL,
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       "_Cancel", GTK_RESPONSE_CANCEL,
                                       "_Export", GTK_RESPONSE_ACCEPT,
                                       NULL);
       gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
                                                      TRUE);

       g_signal_connect(G_OBJECT(d), "response",
                        G_CALLBACK(export_pdf_response_sig), p);

       gtk_widget_show_all(d);
}



GActionEntry nw_entries[] = {

	{ "save", save_sig, NULL, NULL, NULL },
	{ "saveas", saveas_sig, NULL, NULL, NULL },
	{ "sorter", open_slidesorter_sig, NULL, NULL, NULL },
	{ "deleteframe", delete_frame_sig, NULL, NULL, NULL },
	{ "slide", add_slide_sig, NULL, NULL, NULL },
	{ "startslideshow", start_slideshow_sig, NULL, NULL, NULL },
	{ "notes", open_notes_sig, NULL, NULL, NULL },
	{ "clock", open_clock_sig, NULL, NULL, NULL },
	{ "testcard", testcard_sig, NULL, NULL, NULL },
	{ "first", first_slide_sig, NULL, NULL, NULL },
	{ "prev", prev_slide_sig, NULL, NULL, NULL },
	{ "next", next_slide_sig, NULL, NULL, NULL },
	{ "last", last_slide_sig, NULL, NULL, NULL },
};


GActionEntry nw_entries_p[] = {
	{ "print", print_sig, NULL, NULL, NULL  },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },
};


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 NarrativeWindow *nw)
{
	return 0;
}


static void scroll_down(NarrativeWindow *nw)
{
	gdouble inc, val;
	GtkAdjustment *vadj;

	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(nw->sceditor));
	inc = gtk_adjustment_get_step_increment(GTK_ADJUSTMENT(vadj));
	val = gtk_adjustment_get_value(GTK_ADJUSTMENT(vadj));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(vadj), inc+val);
}


static gboolean destroy_sig(GtkWidget *da, NarrativeWindow *nw)
{
	g_application_release(nw->app);
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              NarrativeWindow *nw)
{
	switch ( event->keyval ) {

		case GDK_KEY_B :
		case GDK_KEY_b :
		if ( nw->show != NULL ) {
			scroll_down(nw);
			return TRUE;
		}
		break;

		case GDK_KEY_Page_Up :
		if ( nw->show != NULL ) {
			ss_prev_slide(nw->show, nw);
			return TRUE;
		}
		break;

		case GDK_KEY_Page_Down :
		if ( nw->show != NULL) {
			ss_next_slide(nw->show, nw);
			return TRUE;
		}
		break;

	}

	return FALSE;
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


static SCBlock *narrative_stylesheet()
{
	return sc_parse("\\stylesheet{"
	                "\\ss[slide]{\\callback[sthumb]}"
	                "}");
}


static int create_thumbnail(SCInterpreter *scin, SCBlock *bl,
                            double *w, double *h, void **bvp, void *vp)
{
	SCBlock *b;

	*w = 320.0;
	*h = 256.0;
	b = sc_interp_get_macro_real_block(scin);

	*bvp = b;

	return 1;
}


static cairo_surface_t *render_thumbnail(int w, int h, void *bvp, void *vp)
{
	struct presentation *p = vp;
	SCBlock *scblocks = bvp;
	cairo_surface_t *surf;
	SCBlock *stylesheets[2];
	struct frame *top;

	scblocks = sc_block_child(scblocks);
	stylesheets[0] = p->stylesheet;
	stylesheets[1] = NULL;
	/* FIXME: Cache like crazy here */
	surf = render_sc(scblocks, w, h, 1024.0, 768.0, stylesheets, NULL,
	                 p->is, ISZ_THUMBNAIL, 0, &top, p->lang);
	frame_free(top);

	return surf;
}


static int click_thumbnail(double x, double y, void *bvp, void *vp)
{
	struct presentation *p = vp;
	SCBlock *scblocks = bvp;

	slide_window_open(p, scblocks, p->narrative_window->app);

	return 0;
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
	GtkWidget *image;

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
	g_action_map_add_action_entries(G_ACTION_MAP(nw->window), nw_entries_p,
	                                G_N_ELEMENTS(nw_entries_p), p);

	nw_update_titlebar(nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw->window), vbox);

	if ( p->stylesheet != NULL ) {
		stylesheets[0] = p->stylesheet;
		stylesheets[1] = narrative_stylesheet();
		stylesheets[2] = NULL;
	} else {
		stylesheets[0] = narrative_stylesheet();
		stylesheets[1] = NULL;
	}

	if ( nw->p->scblocks == NULL ) {
		nw->p->scblocks = sc_parse("");
	}

	nw->sceditor = sc_editor_new(nw->p->scblocks, stylesheets, p->lang);
	cbl = sc_callback_list_new();
	sc_callback_list_add_callback(cbl, "sthumb", create_thumbnail,
	                              render_thumbnail, click_thumbnail, p);
	sc_editor_set_callbacks(nw->sceditor, cbl);

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
	nw->bfirst = gtk_tool_button_new(image, "First slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bfirst));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bfirst),
	                               "win.first");

	image = gtk_image_new_from_icon_name("gtk-go-back-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bprev = gtk_tool_button_new(image, "Previous slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bprev));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bprev),
	                               "win.prev");

	image = gtk_image_new_from_icon_name("gtk-go-forward-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bnext = gtk_tool_button_new(image, "Next slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bnext));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bnext),
	                               "win.next");

	image = gtk_image_new_from_icon_name("gtk-goto-last-ltr",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->blast = gtk_tool_button_new(image, "Last slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->blast));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->blast),
	                               "win.last");

	nw->sel_slide = NULL;
	update_toolbar(nw);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(nw->sceditor));

	sc_editor_set_flow(nw->sceditor, 1);
	sc_editor_set_background(nw->sceditor, 0.9, 0.9, 0.9);
	sc_editor_set_min_border(nw->sceditor, 0.0);
	sc_editor_set_top_frame_editable(nw->sceditor, 1);

	g_signal_connect(G_OBJECT(nw->sceditor), "button-press-event",
	                 G_CALLBACK(button_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->sceditor), "key-press-event",
			 G_CALLBACK(key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->window), "destroy",
			 G_CALLBACK(destroy_sig), nw);

	gtk_window_set_default_size(GTK_WINDOW(nw->window), 768, 768);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all(nw->window);
	nw->app = app;
	g_application_hold(app);

	return nw;
}
