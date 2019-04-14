/*
 * narrative_window.c
 *
 * Copyright © 2014-2019 Thomas White <taw@bitwiz.org.uk>
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

#include <libintl.h>
#define _(x) gettext(x)

#include <narrative.h>
#include <gtk/gtknarrativeview.h>

#include "colloquium.h"
#include "narrative_window.h"
#include "slide_window.h"
#include "slide_render_cairo.h"
#include "testcard.h"
#include "pr_clock.h"
#include "slideshow.h"
#include "print.h"
//#include "stylesheet_editor.h"

struct _narrative_window
{
	GtkWidget           *window;
	GtkToolItem         *bfirst;
	GtkToolItem         *bprev;
	GtkToolItem         *bnext;
	GtkToolItem         *blast;
	GtkWidget           *nv;
	GApplication        *app;
	Narrative           *n;
	GFile               *file;
	SCSlideshow         *show;
	int                  show_no_slides;
	PRClock             *pr_clock;
	SlideWindow         *slidewindows[16];
	int                  n_slidewindows;
};


static void show_error(NarrativeWindow *nw, const char *err)
{
	GtkWidget *mw;

	mw = gtk_message_dialog_new(GTK_WINDOW(nw->window),
	                            GTK_DIALOG_DESTROY_WITH_PARENT,
	                            GTK_MESSAGE_ERROR,
	                            GTK_BUTTONS_CLOSE, "%s", err);

	g_signal_connect_swapped(mw, "response",
	                         G_CALLBACK(gtk_widget_destroy), mw);

	gtk_widget_show(mw);
}


static char *get_titlebar_string(NarrativeWindow *nw)
{
	if ( nw == NULL || nw->file == NULL ) {
		return strdup(_("(untitled)"));
	} else {
		char *bn = g_file_get_basename(nw->file);
		return bn;
	}
}



static void update_titlebar(NarrativeWindow *nw)
{
	char *title;
	char *title_new;

	title = get_titlebar_string(nw);
	title_new = realloc(title, strlen(title)+16);
	if ( title_new == NULL ) {
		free(title);
		return;
	} else {
		title = title_new;
	}

	strcat(title, " - Colloquium");
	if ( narrative_get_unsaved(nw->n) ) {
		strcat(title, " *");
	}
	gtk_window_set_title(GTK_WINDOW(nw->window), title);

	/* FIXME: Update all slide windows belonging to this NW */

	free(title);
}


static void update_toolbar(NarrativeWindow *nw)
{
	int cur_para, n_para;

	cur_para = gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv));
	if ( cur_para == 0 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), TRUE);
	}

	n_para = narrative_get_num_items(nw->n);
	if ( cur_para == n_para - 1 ) {
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

		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));

		if ( narrative_save(nw->n, file) ) {
			show_error(nw, _("Failed to save presentation"));
		} else {
			update_titlebar(nw);
		}

		if ( nw->file != file ) {
			if ( nw->file != NULL ) g_object_unref(nw->file);
			nw->file = file;
			g_object_ref(nw->file);
		}
		g_object_unref(file);

	}
	gtk_widget_destroy(d);
	return 0;
}


static void saveas_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GtkWidget *d;
	GtkWidget *box;
	NarrativeWindow *nw = vp;

	d = gtk_file_chooser_dialog_new(_("Save presentation"),
	                                GTK_WINDOW(nw->window),
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                _("_Save"), GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(d), box);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(saveas_response_sig), nw);

	gtk_widget_show_all(d);
}


static void about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	open_about_dialog(nw->window);
}


static void save_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;

	if ( nw->file == NULL ) {
		return saveas_sig(NULL, NULL, nw);
	}

	if ( narrative_save(nw->n, nw->file) ) {
		show_error(nw, _("Failed to save presentation"));
	} else {
		update_titlebar(nw);
	}
}


static void delete_slide_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	/* FIXME: GtkNarrativeView hooks */
//	SCBlock *ns;
//	NarrativeWindow *nw = vp;
//
//	/* Get the SCBlock corresponding to the slide */
//	ns = sc_editor_get_cursor_bvp(nw->nv);
//	if ( ns == NULL ) {
//		fprintf(stderr, "Not a slide!\n");
//		return;
//	}
//
//	sc_block_delete(&nw->dummy_top, ns);
//
//	/* Full rerender */
//	sc_editor_set_scblock(nw->nv, nw->dummy_top);
//	nw->n->saved = 0;
//	update_titlebar(nw);
}


static gint load_ss_response_sig(GtkWidget *d, gint response,
                                 NarrativeWindow *nw)
{
//	if ( response == GTK_RESPONSE_ACCEPT ) {
//
//		GFile *file;
//		Stylesheet *new_ss;
//
//		file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));
//
//		new_ss = stylesheet_load(file);
//		if ( new_ss != NULL ) {
//
//			stylesheet_free(nw->p->stylesheet);
//			nw->n->stylesheet = new_ss;
//			sc_editor_set_stylesheet(nw->nv, new_ss);
//
//			/* Full rerender */
//			sc_editor_set_scblock(nw->nv, nw->dummy_top);
//
//		} else {
//			fprintf(stderr, _("Failed to load stylesheet\n"));
//		}
//
//		g_object_unref(file);
//
//	}
//
//	gtk_widget_destroy(d);

	return 0;
}


static void stylesheet_changed_sig(GtkWidget *da, NarrativeWindow *nw)
{
//	int i;
//
//	/* It might have changed (been created) since last time */
//	sc_editor_set_stylesheet(nw->nv, nwn>p->stylesheet);
//
//	/* Full rerender, first block may have changed */
//	sc_editor_set_scblock(nw->nv, nw->dummy_top);
//
//	/* Full rerender of all slide windows */
//	for ( i=0; i<nw->n_slidewindows; i++ ) {
//		slide_window_update(nw->slidewindows[i]);
//	}
}


static void edit_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
//	NarrativeWindow *nw = vp;
//	StylesheetEditor *se;
//
//	se = stylesheet_editor_new(nw->n);
//	gtk_window_set_transient_for(GTK_WINDOW(se), GTK_WINDOW(nw->window));
//	g_signal_connect(G_OBJECT(se), "changed",
//	                 G_CALLBACK(stylesheet_changed_sig), nw);
//	gtk_widget_show_all(GTK_WIDGET(se));
}


static void load_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
//	//SCBlock *nsblock;
//	//SCBlock *templ;
//	NarrativeWindow *nw = vp;
//	GtkWidget *d;
//
//	d = gtk_file_chooser_dialog_new(_("Load stylesheet"),
//	                                GTK_WINDOW(nw->window),
//	                                GTK_FILE_CHOOSER_ACTION_OPEN,
//	                                _("_Cancel"), GTK_RESPONSE_CANCEL,
//	                                _("_Open"), GTK_RESPONSE_ACCEPT,
//	                                NULL);
//
//	g_signal_connect(G_OBJECT(d), "response",
//	                 G_CALLBACK(load_ss_response_sig), nw);
//
//	gtk_widget_show_all(d);
}


static void add_slide_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_slide_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void first_para_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	NarrativeWindow *nw = vp;
	int n_paras = narrative_get_num_items(nw->n);
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv), 0);
	pr_clock_set_pos(nw->pr_clock,
	                 gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv)),
	                 n_paras);
	update_toolbar(nw);
}


static void ss_prev_para(SCSlideshow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	int n_paras = narrative_get_num_items(nw->n);
	if ( gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv)) == 0 ) return;
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv),
	                          gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv))-1);
	pr_clock_set_pos(nw->pr_clock,
	                 gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv)),
	                 n_paras);
	update_toolbar(nw);
}


static void prev_para_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	ss_prev_para(nw->show, nw);
}


static void ss_next_para(SCSlideshow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	Slide *ns;
	GtkNarrativeView *nv;
	int n_paras;

	n_paras = narrative_get_num_items(nw->n);
	nv = GTK_NARRATIVE_VIEW(nw->nv);

	if ( gtk_narrative_view_get_cursor_para(nv) == n_paras - 1 ) return;
	gtk_narrative_view_set_cursor_para(nv, gtk_narrative_view_get_cursor_para(nv)+1);

	/* If we only have one monitor, skip to next slide */
	if ( ss->single_monitor && !nw->show_no_slides ) {
		int i;
		for ( i=gtk_narrative_view_get_cursor_para(nv); i<n_paras; i++ )
		{
			Slide *ns;
			gtk_narrative_view_set_cursor_para(nv, i);
			ns = narrative_get_slide(nw->n, i);
			if ( ns != NULL ) break;
		}
	}

	pr_clock_set_pos(nw->pr_clock, gtk_narrative_view_get_cursor_para(nv), n_paras);
	ns = narrative_get_slide(nw->n, gtk_narrative_view_get_cursor_para(nv));
	if ( ns != NULL ) {
		sc_slideshow_set_slide(nw->show, ns);
	}
	update_toolbar(nw);
}


static void next_para_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	ss_next_para(nw->show, nw);
}


static void last_para_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	int n_paras = narrative_get_num_items(nw->n);
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv), -1);
	pr_clock_set_pos(nw->pr_clock,
	                 gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv)),
	                 n_paras);
	update_toolbar(nw);
}


static void open_clock_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	if ( nw->pr_clock != NULL ) return;
	nw->pr_clock = pr_clock_new(&nw->pr_clock);
}


static void testcard_sig(GSimpleAction *action, GVariant *parameter,
                         gpointer vp)
{
	NarrativeWindow *nw = vp;
	show_testcard(nw->n);
}


static gint export_pdf_response_sig(GtkWidget *d, gint response,
                                    Narrative *n)
{
       if ( response == GTK_RESPONSE_ACCEPT ) {
               char *filename;
               filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
               render_slides_to_pdf(n, narrative_get_imagestore(n), filename);
               g_free(filename);
       }

       gtk_widget_destroy(d);

       return 0;
}


static void print_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	run_printing(nw->n, nw->window);
}


static void exportpdf_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new(_("Export PDF"),
	                                NULL,
	                                GTK_FILE_CHOOSER_ACTION_SAVE,
	                                _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                _("_Export"), GTK_RESPONSE_ACCEPT,
	                                NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d),
	                                               TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(export_pdf_response_sig), nw->n);

	gtk_widget_show_all(d);
}



static gboolean nw_button_press_sig(GtkWidget *da, GdkEventButton *event,
                                    NarrativeWindow *nw)
{
	return 0;
}


static void changed_sig(GtkWidget *da, NarrativeWindow *nw)
{
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void scroll_down(NarrativeWindow *nw)
{
	gdouble inc, val;
	GtkAdjustment *vadj;

	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(nw->nv));
	inc = gtk_adjustment_get_step_increment(GTK_ADJUSTMENT(vadj));
	val = gtk_adjustment_get_value(GTK_ADJUSTMENT(vadj));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(vadj), inc+val);
}


static gboolean nw_destroy_sig(GtkWidget *da, NarrativeWindow *nw)
{
	if ( nw->pr_clock != NULL ) pr_clock_destroy(nw->pr_clock);
	g_application_release(nw->app);
	return FALSE;
}


static gboolean nw_double_click_sig(GtkWidget *da, gpointer *pslide,
                                    NarrativeWindow *nw)
{
	Slide *slide = (Slide *)pslide;
	if ( nw->show == NULL ) {
		slide_window_open(nw->n, slide, nw->app);
	} else {
		sc_slideshow_set_slide(nw->show, slide);
	}
	return FALSE;
}


static gboolean nw_key_press_sig(GtkWidget *da, GdkEventKey *event,
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
			ss_prev_para(nw->show, nw);
			return TRUE;
		}
		break;

		case GDK_KEY_Page_Down :
		if ( nw->show != NULL) {
			ss_next_para(nw->show, nw);
			return TRUE;
		}
		break;

		case GDK_KEY_Escape :
		if ( nw->show != NULL ) {
			gtk_widget_destroy(GTK_WIDGET(nw->show));
			return TRUE;
		}
		break;

		case GDK_KEY_F5 :
		if ( nw->show != NULL ) {
			/* Trap F5 so that full rerender does NOT happen */
			return TRUE;
		}

	}

	return FALSE;
}


static gboolean ss_destroy_sig(GtkWidget *da, NarrativeWindow *nw)
{
	nw->show = NULL;
	gtk_narrative_view_set_para_highlight(GTK_NARRATIVE_VIEW(nw->nv), 0);

	gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);

	return FALSE;
}


static void start_slideshow_here_sig(GSimpleAction *action, GVariant *parameter,
                                     gpointer vp)
{
	NarrativeWindow *nw = vp;
	Slide *slide;

	if ( narrative_get_num_slides(nw->n) == 0 ) return;

	slide = narrative_get_slide(nw->n,
	                            gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv)));
	if ( slide == NULL ) return;

	nw->show = sc_slideshow_new(nw->n, GTK_APPLICATION(nw->app));
	if ( nw->show == NULL ) return;

	nw->show_no_slides = 0;

	g_signal_connect(G_OBJECT(nw->show), "key-press-event",
		 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->show), "destroy",
		 G_CALLBACK(ss_destroy_sig), nw);
	sc_slideshow_set_slide(nw->show, slide);
	gtk_narrative_view_set_para_highlight(GTK_NARRATIVE_VIEW(nw->nv), 1);
	gtk_widget_show_all(GTK_WIDGET(nw->show));
	update_toolbar(nw);
}


static void start_slideshow_noslides_sig(GSimpleAction *action, GVariant *parameter,
                                         gpointer vp)
{
	NarrativeWindow *nw = vp;

	if ( narrative_get_num_slides(nw->n) == 0 ) return;

	nw->show = sc_slideshow_new(nw->n, GTK_APPLICATION(nw->app));
	if ( nw->show == NULL ) return;

	nw->show_no_slides = 1;

	g_signal_connect(G_OBJECT(nw->show), "key-press-event",
		 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->show), "destroy",
		 G_CALLBACK(ss_destroy_sig), nw);
	sc_slideshow_set_slide(nw->show, narrative_get_slide_by_number(nw->n, 0));
	gtk_narrative_view_set_para_highlight(GTK_NARRATIVE_VIEW(nw->nv), 1);
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv), 0);
	update_toolbar(nw);
}


static void start_slideshow_sig(GSimpleAction *action, GVariant *parameter,
                                gpointer vp)
{
	NarrativeWindow *nw = vp;

	if ( narrative_get_num_slides(nw->n) == 0 ) return;

	nw->show = sc_slideshow_new(nw->n, GTK_APPLICATION(nw->app));
	if ( nw->show == NULL ) return;

	nw->show_no_slides = 0;

	g_signal_connect(G_OBJECT(nw->show), "key-press-event",
		 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->show), "destroy",
		 G_CALLBACK(ss_destroy_sig), nw);
	sc_slideshow_set_slide(nw->show, narrative_get_slide_by_number(nw->n, 0));
	gtk_narrative_view_set_para_highlight(GTK_NARRATIVE_VIEW(nw->nv), 1);
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv), 0);
	gtk_widget_show_all(GTK_WIDGET(nw->show));
	update_toolbar(nw);
}


GActionEntry nw_entries[] = {

	{ "about", about_sig, NULL, NULL, NULL },
	{ "save", save_sig, NULL, NULL, NULL },
	{ "saveas", saveas_sig, NULL, NULL, NULL },
	{ "deleteslide", delete_slide_sig, NULL, NULL, NULL },
	{ "slide", add_slide_sig, NULL, NULL, NULL },
	{ "loadstylesheet", load_ss_sig, NULL, NULL, NULL },
	{ "stylesheet", edit_ss_sig, NULL, NULL, NULL },
	{ "startslideshow", start_slideshow_sig, NULL, NULL, NULL },
	{ "startslideshowhere", start_slideshow_here_sig, NULL, NULL, NULL },
	{ "startslideshownoslides", start_slideshow_noslides_sig, NULL, NULL, NULL },
	{ "clock", open_clock_sig, NULL, NULL, NULL },
	{ "testcard", testcard_sig, NULL, NULL, NULL },
	{ "first", first_para_sig, NULL, NULL, NULL },
	{ "prev", prev_para_sig, NULL, NULL, NULL },
	{ "next", next_para_sig, NULL, NULL, NULL },
	{ "last", last_para_sig, NULL, NULL, NULL },
	{ "print", print_sig, NULL, NULL, NULL  },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },
};


//void narrative_window_sw_closed(NarrativeWindow *nw, SlideWindow *sw)
//{
//	int i;
//	int found = 0;
//
//	for ( i=0; i<nw->n_slidewindows; i++ ) {
//		if ( nw->slidewindows[i] == sw ) {
//
//			int j;
//			for ( j=i; j<nw->n_slidewindows-1; j++ ) {
//				nw->slidewindows[j] = nw->slidewindows[j+1];
//			}
//			nw->n_slidewindows--;
//			found = 1;
//		}
//	}
//
//	if ( !found ) {
//		fprintf(stderr, "Couldn't find slide window in narrative record\n");
//	}
//}


NarrativeWindow *narrative_window_new(Narrative *n, GFile *file, GApplication *papp)
{
	NarrativeWindow *nw;
	GtkWidget *vbox;
	GtkWidget *scroll;
	GtkWidget *toolbar;
	GtkToolItem *button;
	GtkWidget *image;
	Colloquium *app = COLLOQUIUM(papp);

	nw = calloc(1, sizeof(NarrativeWindow));
	if ( nw == NULL ) return NULL;

	nw->app = papp;
	nw->n = n;
	nw->n_slidewindows = 0;
	nw->file = file;
	if ( file != NULL ) g_object_ref(file);

	nw->window = gtk_application_window_new(GTK_APPLICATION(app));
	update_titlebar(nw);

	g_action_map_add_action_entries(G_ACTION_MAP(nw->window), nw_entries,
	                                G_N_ELEMENTS(nw_entries), nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw->window), vbox);

	nw->nv = gtk_narrative_view_new(n);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(toolbar), FALSE, FALSE, 0);

	/* Fullscreen */
	image = gtk_image_new_from_icon_name("view-fullscreen",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, _("Start slideshow"));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.startslideshow");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	/* Add slide */
	image = gtk_image_new_from_icon_name("list-add",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, _("Add slide"));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	image = gtk_image_new_from_icon_name("go-top",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bfirst = gtk_tool_button_new(image, _("First slide"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bfirst));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bfirst),
	                               "win.first");

	image = gtk_image_new_from_icon_name("go-up",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bprev = gtk_tool_button_new(image, _("Previous slide"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bprev));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bprev),
	                               "win.prev");

	image = gtk_image_new_from_icon_name("go-down",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bnext = gtk_tool_button_new(image, _("Next slide"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bnext));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bnext),
	                               "win.next");

	image = gtk_image_new_from_icon_name("go-bottom",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->blast = gtk_tool_button_new(image, _("Last slide"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->blast));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->blast),
	                               "win.last");

	update_toolbar(nw);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	                               GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(nw->nv));

	g_signal_connect(G_OBJECT(nw->nv), "button-press-event",
	                 G_CALLBACK(nw_button_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->nv), "changed",
	                 G_CALLBACK(changed_sig), nw);
	g_signal_connect(G_OBJECT(nw->nv), "key-press-event",
			 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->window), "destroy",
			 G_CALLBACK(nw_destroy_sig), nw);
	g_signal_connect(G_OBJECT(nw->nv), "slide-double-clicked",
			 G_CALLBACK(nw_double_click_sig), nw);

	gtk_window_set_default_size(GTK_WINDOW(nw->window), 768, 768);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
	gtk_container_set_focus_child(GTK_CONTAINER(nw->window),
	                              GTK_WIDGET(nw->nv));

	gtk_widget_show_all(nw->window);
	g_application_hold(papp);

	return nw;
}
