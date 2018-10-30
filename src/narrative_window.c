/*
 * narrative_window.c
 *
 * Copyright © 2014-2018 Thomas White <taw@bitwiz.org.uk>
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

#include "colloquium.h"
#include "presentation.h"
#include "narrative_window.h"
#include "sc_editor.h"
#include "sc_parse.h"
#include "render.h"
#include "testcard.h"
#include "pr_clock.h"
#include "print.h"
#include "utils.h"
#include "stylesheet_editor.h"


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
	SCBlock             *dummy_top;
	SCSlideshow         *show;
	int                  show_no_slides;
	PRClock             *pr_clock;
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


static void update_toolbar(NarrativeWindow *nw)
{
	int cur_para;

	cur_para = sc_editor_get_cursor_para(nw->sceditor);
	if ( cur_para == 0 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), TRUE);
	}

	if ( cur_para == sc_editor_get_num_paras(nw->sceditor)-1 ) {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), TRUE);
	}
}


struct saveas_info
{
	NarrativeWindow *nw;

	/* Radio buttons for how to save stylesheet */
	GtkWidget *privatess;
	GtkWidget *folderss;
	GtkWidget *noss;
};


static gint saveas_response_sig(GtkWidget *d, gint response,
                                struct saveas_info *si)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));
		GFile *ssfile = NULL;

		if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(si->privatess)) ) {
			gchar *ssuri;
			ssuri = g_file_get_uri(file);
			if ( ssuri != NULL ) {
				size_t l = strlen(ssuri);
				if ( ssuri[l-3] == '.' && ssuri[l-2] == 's' && ssuri[l-1] =='c' ) {
					ssuri[l-1] = 's';
					ssfile = g_file_new_for_uri(ssuri);
					g_free(ssuri);
				}
			}
		} else if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(si->folderss)) ) {
			GFile *parent;
			parent = g_file_get_parent(file);
			if ( parent != NULL ) {
				ssfile = g_file_get_child(parent, "stylesheet.ss");
				g_object_unref(parent);
			}
		} else if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(si->noss)) ) {
			/* Do nothing */
		} else {
			fprintf(stderr, _("Couldn't determine how to save stylesheet!\n"));
		}

		if ( save_presentation(si->nw->p, file, ssfile) ) {
			show_error(si->nw, _("Failed to save presentation"));
		}

		/* save_presentation keeps a reference to both of these */
		g_object_unref(file);
		if ( ssfile != NULL ) g_object_unref(ssfile);

	}

	gtk_widget_destroy(d);
	free(si);

	return 0;
}


static void saveas_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GtkWidget *d;
	GtkWidget *box;
	NarrativeWindow *nw = vp;
	struct saveas_info *si;

	si = malloc(sizeof(struct saveas_info));
	if ( si == NULL ) return;

	si->nw = nw;

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
	si->privatess = gtk_radio_button_new_with_label(NULL,
	     _("Create/update private stylesheet for this presentation"));
	gtk_box_pack_start(GTK_BOX(box), si->privatess, FALSE, FALSE, 0);
	si->folderss = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(si->privatess),
	     _("Create/update default stylesheet for presentations in folder"));
	gtk_box_pack_start(GTK_BOX(box), si->folderss, FALSE, FALSE, 0);
	si->noss = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(si->privatess),
	     _("Don't save stylesheet at all"));
	gtk_box_pack_start(GTK_BOX(box), si->noss, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(si->privatess), TRUE);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(saveas_response_sig), si);

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

	if ( nw->p->file == NULL ) {
		return saveas_sig(NULL, NULL, nw);
	}

	save_presentation(nw->p, nw->p->file, nw->p->stylesheet_from);
}


static void delete_slide_sig(GSimpleAction *action, GVariant *parameter,
                              gpointer vp)
{
	SCBlock *ns;
	NarrativeWindow *nw = vp;

	/* Get the SCBlock corresponding to the slide */
	ns = sc_editor_get_cursor_bvp(nw->sceditor);
	if ( ns == NULL ) {
		fprintf(stderr, _("Not a slide!\n"));
		return;
	}

	sc_block_delete(&nw->dummy_top, ns);

	/* Full rerender */
	sc_editor_set_scblock(nw->sceditor, nw->dummy_top);
	nw->p->saved = 0;
	update_titlebar(nw);
}


static gint load_ss_response_sig(GtkWidget *d, gint response,
                                 NarrativeWindow *nw)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		GFile *file;
		Stylesheet *new_ss;

		file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));

		new_ss = stylesheet_load(file);
		if ( new_ss != NULL ) {

			stylesheet_free(nw->p->stylesheet);
			nw->p->stylesheet = new_ss;
			sc_editor_set_stylesheet(nw->sceditor, new_ss);

			/* Full rerender */
			sc_editor_set_scblock(nw->sceditor, nw->dummy_top);

		} else {
			fprintf(stderr, _("Failed to load\n"));
		}

		g_object_unref(file);

	}

	gtk_widget_destroy(d);

	return 0;
}


static void stylesheet_changed_sig(GtkWidget *da, NarrativeWindow *nw)
{
	/* It might have changed (been created) since last time */
	sc_editor_set_stylesheet(nw->sceditor, nw->p->stylesheet);

	/* Full rerender, first block may have changed */
	sc_editor_set_scblock(nw->sceditor, nw->dummy_top);
}


static void edit_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
	NarrativeWindow *nw = vp;
	StylesheetEditor *se;

	se = stylesheet_editor_new(nw->p);
	gtk_window_set_transient_for(GTK_WINDOW(se), GTK_WINDOW(nw->window));
	g_signal_connect(G_OBJECT(se), "changed",
	                 G_CALLBACK(stylesheet_changed_sig), nw);
	gtk_widget_show_all(GTK_WIDGET(se));
}


static void load_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
	//SCBlock *nsblock;
	//SCBlock *templ;
	NarrativeWindow *nw = vp;
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new(_("Load stylesheet"),
	                                GTK_WINDOW(nw->window),
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
	SCBlock *nsblock;
	SCBlock *templ;
	NarrativeWindow *nw = vp;

	sc_editor_ensure_cursor(nw->sceditor);

	/* Split the current paragraph */
	nsblock = split_paragraph_at_cursor(nw->sceditor);

	/* FIXME: Template from JSON */
	templ = sc_parse("\\slide{}");

	/* Link the new SCBlock in */
	if ( nsblock != NULL ) {
		sc_block_append_p(nsblock, templ);
	} else {
		fprintf(stderr, _("Failed to split paragraph\n"));
	}

	sc_editor_set_scblock(nw->sceditor, nw->dummy_top);
	nw->p->saved = 0;
	update_titlebar(nw);
}


static void first_para_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
	NarrativeWindow *nw = vp;
	sc_editor_set_cursor_para(nw->sceditor, 0);
	pr_clock_set_pos(nw->pr_clock, sc_editor_get_cursor_para(nw->sceditor),
	                               sc_editor_get_num_paras(nw->sceditor));
	update_toolbar(nw);
}


static void ss_prev_para(SCSlideshow *ss, void *vp)
{
	NarrativeWindow *nw = vp;
	if ( sc_editor_get_cursor_para(nw->sceditor) == 0 ) return;
	sc_editor_set_cursor_para(nw->sceditor,
	                          sc_editor_get_cursor_para(nw->sceditor)-1);
	pr_clock_set_pos(nw->pr_clock, sc_editor_get_cursor_para(nw->sceditor),
	                               sc_editor_get_num_paras(nw->sceditor));
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
	SCBlock *ns;

	sc_editor_set_cursor_para(nw->sceditor,
	                          sc_editor_get_cursor_para(nw->sceditor)+1);

	/* If we only have one monitor, don't try to do paragraph counting */
	if ( ss->single_monitor && !nw->show_no_slides ) {
		int i, max;
		max = sc_editor_get_num_paras(nw->sceditor);
		for ( i=sc_editor_get_cursor_para(nw->sceditor); i<max; i++ ) {
			SCBlock *ns;
			sc_editor_set_cursor_para(nw->sceditor, i);
			ns = sc_editor_get_cursor_bvp(nw->sceditor);
			if ( ns != NULL ) break;
		}
	}

	pr_clock_set_pos(nw->pr_clock, sc_editor_get_cursor_para(nw->sceditor),
	                               sc_editor_get_num_paras(nw->sceditor));
	ns = sc_editor_get_cursor_bvp(nw->sceditor);
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
	sc_editor_set_cursor_para(nw->sceditor, -1);
	pr_clock_set_pos(nw->pr_clock, sc_editor_get_cursor_para(nw->sceditor),
	                               sc_editor_get_num_paras(nw->sceditor));
	update_toolbar(nw);
}


static void open_clock_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	nw->pr_clock = pr_clock_new();
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


static void print_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	NarrativeWindow *nw = vp;
	run_printing(nw->p, nw->window);
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
	                 G_CALLBACK(export_pdf_response_sig), nw->p);

	gtk_widget_show_all(d);
}



static gboolean nw_button_press_sig(GtkWidget *da, GdkEventButton *event,
                                    NarrativeWindow *nw)
{
	return 0;
}


static void changed_sig(GtkWidget *da, NarrativeWindow *nw)
{
	nw->p->saved = 0;
	update_titlebar(nw);
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


static gboolean nw_destroy_sig(GtkWidget *da, NarrativeWindow *nw)
{
	g_application_release(nw->app);
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
	sc_editor_set_para_highlight(nw->sceditor, 0);

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
	void *bvp;

	if ( num_slides(nw->p) == 0 ) return;

	bvp = sc_editor_get_cursor_bvp(nw->sceditor);
	if ( bvp == NULL ) return;

	nw->show = sc_slideshow_new(nw->p, GTK_APPLICATION(nw->app));
	if ( nw->show == NULL ) return;

	nw->show_no_slides = 0;

	g_signal_connect(G_OBJECT(nw->show), "key-press-event",
		 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->show), "destroy",
		 G_CALLBACK(ss_destroy_sig), nw);
	sc_slideshow_set_slide(nw->show, bvp);
	sc_editor_set_para_highlight(nw->sceditor, 1);
	gtk_widget_show_all(GTK_WIDGET(nw->show));
	update_toolbar(nw);
}


static void start_slideshow_noslides_sig(GSimpleAction *action, GVariant *parameter,
                                         gpointer vp)
{
	NarrativeWindow *nw = vp;

	if ( num_slides(nw->p) == 0 ) return;

	nw->show = sc_slideshow_new(nw->p, GTK_APPLICATION(nw->app));
	if ( nw->show == NULL ) return;

	nw->show_no_slides = 1;

	g_signal_connect(G_OBJECT(nw->show), "key-press-event",
		 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->show), "destroy",
		 G_CALLBACK(ss_destroy_sig), nw);
	sc_slideshow_set_slide(nw->show, first_slide(nw->p));
	sc_editor_set_para_highlight(nw->sceditor, 1);
	sc_editor_set_cursor_para(nw->sceditor, 0);
	update_toolbar(nw);
}


static void start_slideshow_sig(GSimpleAction *action, GVariant *parameter,
                                gpointer vp)
{
	NarrativeWindow *nw = vp;

	if ( num_slides(nw->p) == 0 ) return;

	nw->show = sc_slideshow_new(nw->p, GTK_APPLICATION(nw->app));
	if ( nw->show == NULL ) return;

	nw->show_no_slides = 0;

	g_signal_connect(G_OBJECT(nw->show), "key-press-event",
		 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->show), "destroy",
		 G_CALLBACK(ss_destroy_sig), nw);
	sc_slideshow_set_slide(nw->show, first_slide(nw->p));
	sc_editor_set_para_highlight(nw->sceditor, 1);
	sc_editor_set_cursor_para(nw->sceditor, 0);
	gtk_widget_show_all(GTK_WIDGET(nw->show));
	update_toolbar(nw);
}


static void nw_update_titlebar(NarrativeWindow *nw)
{
	char *tb = get_titlebar_string(nw->p);

	if ( nw->p->slidewindow != NULL ) {

		char *title;

		title = malloc(strlen(tb)+14);
		sprintf(title, "%s - Colloquium", tb);
		gtk_window_set_title(GTK_WINDOW(nw->window), title);
		free(title);

       }

}


static int create_thumbnail(SCInterpreter *scin, SCBlock *bl,
                            double *w, double *h, void **bvp, void *vp)
{
	struct presentation *p = vp;

	*w = 270.0*(p->slide_width / p->slide_height);
	*h = 270.0;
	*bvp = bl;

	return 1;
}


static cairo_surface_t *render_thumbnail(int w, int h, void *bvp, void *vp)
{
	struct presentation *p = vp;
	SCBlock *scblocks = bvp;
	cairo_surface_t *surf;
	struct frame *top;
	int sn = slide_number(p, scblocks);

	/* FIXME: Cache like crazy here */
	surf = render_sc(scblocks, w, h, p->slide_width, p->slide_height,
	                 p->stylesheet, NULL, p->is, sn, &top, p->lang);
	frame_free(top);

	return surf;
}


static int click_thumbnail(double x, double y, void *bvp, void *vp)
{
	struct presentation *p = vp;
	SCBlock *scblocks = bvp;

	if ( p->narrative_window->show != NULL ) {
		sc_slideshow_set_slide(p->narrative_window->show, scblocks);
	} else {
		slide_window_open(p, scblocks, p->narrative_window->app);
	}

	return 0;
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


void update_titlebar(NarrativeWindow *nw)
{
	char *title;
	char *title_new;

	title = get_titlebar_string(nw->p);
	title_new = realloc(title, strlen(title)+16);
	if ( title_new == NULL ) {
		free(title);
		return;
	} else {
		title = title_new;
	}

	strcat(title, " - Colloquium");
	if ( !nw->p->saved ) {
		strcat(title, " *");
	}
	gtk_window_set_title(GTK_WINDOW(nw->window), title);
	free(title);
}


NarrativeWindow *narrative_window_new(struct presentation *p, GApplication *papp)
{
	NarrativeWindow *nw;
	GtkWidget *vbox;
	GtkWidget *scroll;
	GtkWidget *toolbar;
	GtkToolItem *button;
	SCCallbackList *cbl;
	GtkWidget *image;
	Colloquium *app = COLLOQUIUM(papp);

	if ( p->narrative_window != NULL ) {
		fprintf(stderr, _("Narrative window is already open!\n"));
		return NULL;
	}

	nw = calloc(1, sizeof(NarrativeWindow));
	if ( nw == NULL ) return NULL;

	nw->app = papp;
	nw->p = p;

	nw->window = gtk_application_window_new(GTK_APPLICATION(app));
	p->narrative_window = nw;
	update_titlebar(nw);

	g_action_map_add_action_entries(G_ACTION_MAP(nw->window), nw_entries,
	                                G_N_ELEMENTS(nw_entries), nw);

	nw_update_titlebar(nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw->window), vbox);

	/* If the presentation is completely empty, give ourselves at least
	 * something to work with */
	if ( nw->p->scblocks == NULL ) {
		nw->p->scblocks = sc_parse("");
	}

	/* Put everything we have inside \presentation{}.
	 * SCEditor will start processing one level down */
	nw->dummy_top = sc_block_new_parent(nw->p->scblocks, "presentation");

	nw->sceditor = sc_editor_new(nw->dummy_top, p->stylesheet, p->lang,
	                             colloquium_get_imagestore(app));
	cbl = sc_callback_list_new();
	sc_callback_list_add_callback(cbl, "slide", create_thumbnail,
	                              render_thumbnail, click_thumbnail, p);
	sc_editor_set_callbacks(nw->sceditor, cbl);
	sc_editor_set_imagestore(nw->sceditor, p->is);

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
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(nw->sceditor));

	sc_editor_set_flow(nw->sceditor, 1);
	sc_editor_set_background(nw->sceditor, 0.9, 0.9, 0.9);
	sc_editor_set_min_border(nw->sceditor, 0.0);
	sc_editor_set_top_frame_editable(nw->sceditor, 1);

	g_signal_connect(G_OBJECT(nw->sceditor), "button-press-event",
	                 G_CALLBACK(nw_button_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->sceditor), "changed",
	                 G_CALLBACK(changed_sig), nw);
	g_signal_connect(G_OBJECT(nw->sceditor), "key-press-event",
			 G_CALLBACK(nw_key_press_sig), nw);
	g_signal_connect(G_OBJECT(nw->window), "destroy",
			 G_CALLBACK(nw_destroy_sig), nw);

	gtk_window_set_default_size(GTK_WINDOW(nw->window), 768, 768);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
	gtk_container_set_focus_child(GTK_CONTAINER(nw->window),
	                              GTK_WIDGET(nw->sceditor));

	gtk_widget_show_all(nw->window);
	g_application_hold(papp);

	return nw;
}
