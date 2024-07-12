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
#include <gio/gio.h>

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
#include "stylesheet_editor.h"

G_DEFINE_TYPE_WITH_CODE(NarrativeWindow, narrativewindow, GTK_TYPE_APPLICATION_WINDOW, NULL)

static void show_error(NarrativeWindow *nw, const char *err)
{
	GtkWidget *mw;

	mw = gtk_message_dialog_new(GTK_WINDOW(nw),
	                            GTK_DIALOG_DESTROY_WITH_PARENT,
	                            GTK_MESSAGE_ERROR,
	                            GTK_BUTTONS_CLOSE, "%s", err);

	g_signal_connect_swapped(mw, "response",
	                         G_CALLBACK(gtk_widget_destroy), mw);

	gtk_widget_show(mw);
}


char *narrative_window_get_filename(NarrativeWindow *nw)
{
	char *filename;
	if ( nw == NULL || nw->file == NULL ) {
		filename = strdup(_("(untitled)"));
	} else {
		filename = g_file_get_basename(nw->file);
	}
	return filename;
}


static void update_titlebar(NarrativeWindow *nw)
{
	char title[1026];
	char *filename;
	int i;

	filename = narrative_window_get_filename(nw);
	snprintf(title, 1024, "%s - Colloquium", filename);
	if ( narrative_get_unsaved(nw->n) ) strcat(title, " *");
	free(filename);

	gtk_window_set_title(GTK_WINDOW(nw), title);

	for ( i=0; i<nw->n_slidewindows; i++ ) {
		slide_window_update_titlebar(nw->slidewindows[i]);
	}
}


static gboolean slide_window_closed_sig(GtkWidget *sw, GdkEvent *event,
                                        NarrativeWindow *nw)
{
	int i;
	int found = 0;

	for ( i=0; i<nw->n_slidewindows; i++ ) {
		if ( nw->slidewindows[i] == GTK_SLIDE_WINDOW(sw) ) {
			int j;
			for ( j=i; j<nw->n_slidewindows-1; j++ ) {
				nw->slidewindows[j] = nw->slidewindows[j+1];
			}
			nw->n_slidewindows--;
			found = 1;
			break;
		}
	}

	if ( !found ) {
		fprintf(stderr, "Couldn't find slide window in narrative record\n");
	}

	return FALSE;
}


static void update_toolbar(NarrativeWindow *nw)
{
	int cur_para, n_para;

	if ( nw->show == NULL ) return;

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
	                                GTK_WINDOW(nw),
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


static void nw_about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	open_about_dialog(GTK_WIDGET(nw));
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


static gint load_ss_response_sig(GtkWidget *d, gint response,
                                 NarrativeWindow *nw)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		GFile *file;

		file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));

		if ( stylesheet_set_from_file(narrative_get_stylesheet(nw->n), file) ) {

			fprintf(stderr, _("Failed to load stylesheet\n"));

		} else {

			gtk_narrative_view_redraw(GTK_NARRATIVE_VIEW(nw->nv));
		}

		g_object_unref(file);

	}

	gtk_widget_destroy(d);

	return 0;
}


static void stylesheet_changed_sig(GtkWidget *da, NarrativeWindow *nw)
{
	int i;

	/* Full rerender */
	gtk_narrative_view_redraw(GTK_NARRATIVE_VIEW(nw->nv));

	/* Full rerender of all slide windows */
	for ( i=0; i<nw->n_slidewindows; i++ ) {
		slide_window_update(nw->slidewindows[i]);
	}
}


static void edit_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
	NarrativeWindow *nw = vp;
	StylesheetEditor *se;

	se = stylesheet_editor_new(narrative_get_stylesheet(nw->n));
	g_signal_connect(G_OBJECT(se), "changed",
	                 G_CALLBACK(stylesheet_changed_sig), nw);
	gtk_widget_show_all(GTK_WIDGET(se));
}


static void load_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
	NarrativeWindow *nw = vp;
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new(_("Load stylesheet"),
	                                GTK_WINDOW(nw),
	                                GTK_FILE_CHOOSER_ACTION_OPEN,
	                                _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                _("_Open"), GTK_RESPONSE_ACCEPT,
	                                NULL);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(load_ss_response_sig), nw);

	gtk_widget_show_all(d);
}


static void add_slide_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_slide_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}

static void add_prestitle_sig(GSimpleAction *action, GVariant *parameter,
                              gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_prestitle_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void add_bp_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_bp_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void add_segstart_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_segstart_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void add_segend_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_segend_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void add_eop_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_add_eop_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
	narrative_set_unsaved(nw->n);
	update_titlebar(nw);
}


static void set_clock_pos(NarrativeWindow *nw)
{
	int pos = gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv));
	int end = narrative_get_num_items_to_eop(nw->n);
	if ( pos >= end ) pos = end-1;
	pr_clock_set_pos(nw->pr_clock, pos, end);
}


static void first_para_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	NarrativeWindow *nw = vp;
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv), 0);
	set_clock_pos(nw);
	update_toolbar(nw);
}


static void ss_prev_para(SCSlideshow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	if ( gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv)) == 0 ) return;
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv),
	                          gtk_narrative_view_get_cursor_para(GTK_NARRATIVE_VIEW(nw->nv))-1);
	                 set_clock_pos(nw);
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
	if ( ss != NULL && ss->single_monitor && !nw->show_no_slides ) {
		int i;
		for ( i=gtk_narrative_view_get_cursor_para(nv); i<n_paras; i++ )
		{
			Slide *ns;
			gtk_narrative_view_set_cursor_para(nv, i);
			ns = narrative_get_slide(nw->n, i);
			if ( ns != NULL ) break;
		}
	}

	set_clock_pos(nw);
	ns = narrative_get_slide(nw->n, gtk_narrative_view_get_cursor_para(nv));
	if ( nw->show != NULL && ns != NULL ) {
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
	gtk_narrative_view_set_cursor_para(GTK_NARRATIVE_VIEW(nw->nv), -1);
	set_clock_pos(nw);
	update_toolbar(nw);
}


static void set_text_type(NarrativeWindow *nw, enum text_run_type t)
{
	printf("%i\n", t);
}


static void bold_sig(GSimpleAction *action, GVariant *parameter,
                     gpointer vp)
{
	NarrativeWindow *nw = vp;
	set_text_type(nw, TEXT_RUN_BOLD);
}


static void italic_sig(GSimpleAction *action, GVariant *parameter,
                       gpointer vp)
{
	NarrativeWindow *nw = vp;
	set_text_type(nw, TEXT_RUN_ITALIC);
}


static void underline_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
	NarrativeWindow *nw = vp;
	set_text_type(nw, TEXT_RUN_UNDERLINE);
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
	run_printing(nw->n, GTK_WIDGET(nw));
	gtk_narrative_view_redraw(GTK_NARRATIVE_VIEW(nw->nv));
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
	int i;
	if ( nw->pr_clock != NULL ) pr_clock_destroy(nw->pr_clock);
	for ( i=0; i<nw->n_slidewindows; i++ ) {
		gtk_widget_destroy(GTK_WIDGET(nw->slidewindows[i]));
	}
	return FALSE;
}


static gboolean nw_double_click_sig(GtkWidget *da, gpointer *pslide,
                                    NarrativeWindow *nw)
{
	Slide *slide = (Slide *)pslide;
	if ( nw->show == NULL ) {
		if ( nw->n_slidewindows < 16 ) {
			SlideWindow *sw = slide_window_new(nw->n, slide, nw, nw->app);
			nw->slidewindows[nw->n_slidewindows++] = sw;
			g_signal_connect(G_OBJECT(sw), "changed",
					G_CALLBACK(changed_sig), nw);
			g_signal_connect(G_OBJECT(sw), "delete-event",
			                 G_CALLBACK(slide_window_closed_sig), nw);
			gtk_widget_show_all(GTK_WIDGET(sw));
		} else {
			fprintf(stderr, _("Too many slide windows\n"));
		}
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

	{ "about", nw_about_sig, NULL, NULL, NULL },
	{ "save", save_sig, NULL, NULL, NULL },
	{ "saveas", saveas_sig, NULL, NULL, NULL },
	{ "slide", add_slide_sig, NULL, NULL, NULL },
	{ "bp", add_bp_sig, NULL, NULL, NULL },
	{ "eop", add_eop_sig, NULL, NULL, NULL },
	{ "prestitle", add_prestitle_sig, NULL, NULL, NULL },
	{ "segstart", add_segstart_sig, NULL, NULL, NULL },
	{ "segend", add_segend_sig, NULL, NULL, NULL },
	{ "loadstyle", load_ss_sig, NULL, NULL, NULL },
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
	{ "bold", bold_sig, NULL, NULL, NULL },
	{ "italic", italic_sig, NULL, NULL, NULL },
	{ "underline", underline_sig, NULL, NULL, NULL },
	{ "print", print_sig, NULL, NULL, NULL  },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },
};


static void narrativewindow_class_init(NarrativeWindowClass *klass)
{
}


static void narrativewindow_init(NarrativeWindow *sw)
{
}


NarrativeWindow *narrative_window_new(Narrative *n, GFile *file, GApplication *app)
{
	NarrativeWindow *nw;
	GtkWidget *vbox;
	GtkWidget *scroll;
	GtkWidget *toolbar;
	GtkToolItem *button;
	GtkWidget *image;

	nw = g_object_new(GTK_TYPE_NARRATIVE_WINDOW, "application", app, NULL);
	gtk_window_set_role(GTK_WINDOW(nw), "narrative");

	nw->app = app;
	nw->n = n;
	nw->n_slidewindows = 0;
	nw->file = file;
	if ( file != NULL ) g_object_ref(file);

	update_titlebar(nw);

	g_action_map_add_action_entries(G_ACTION_MAP(nw), nw_entries,
	                                G_N_ELEMENTS(nw_entries), nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw), vbox);

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

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	image = gtk_image_new_from_icon_name("format-text-bold",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, _("Bold"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.bold");

	image = gtk_image_new_from_icon_name("format-text-italic",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, _("Italic"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.italic");

	image = gtk_image_new_from_icon_name("format-text-underline",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, _("Underline"));
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.underline");

	gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);

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
	g_signal_connect(G_OBJECT(nw), "destroy",
			 G_CALLBACK(nw_destroy_sig), nw);
	g_signal_connect(G_OBJECT(nw->nv), "slide-double-clicked",
			 G_CALLBACK(nw_double_click_sig), nw);

	gtk_window_set_default_size(GTK_WINDOW(nw), 768, 768);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
	gtk_container_set_focus_child(GTK_CONTAINER(nw),
	                              GTK_WIDGET(nw->nv));

	return nw;
}
