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
	PRClock             *pr_clock;
};


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
	                                "_Save", GTK_RESPONSE_ACCEPT,
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


static void delete_slide_sig(GSimpleAction *action, GVariant *parameter,
                              gpointer vp)
{
	SCBlock *ns;
	NarrativeWindow *nw = vp;

	/* Get the SCBlock corresponding to the slide */
	ns = sc_editor_get_cursor_bvp(nw->sceditor);
	if ( ns == NULL ) {
		fprintf(stderr, "Not a slide!\n");
		return;
	}

	sc_block_delete(&nw->dummy_top, ns);

	/* Full rerender */
	sc_editor_set_scblock(nw->sceditor,
	                      sc_editor_get_scblock(nw->sceditor));
	nw->p->saved = 0;
	update_titlebar(nw);
}


static struct template_id *get_templates(SCBlock *ss, int *n)
{
	struct template_id *list;
	SCInterpreter *scin;

	scin = sc_interp_new(NULL, NULL, NULL, NULL);
	sc_interp_run_stylesheet(scin, ss);  /* ss == NULL is OK */
	list = sc_interp_get_templates(scin, n);
	sc_interp_destroy(scin);
	return list;
}


static void update_template_menus(NarrativeWindow *nw)
{
	struct template_id *templates;
	int i, n_templates;

	templates = get_templates(nw->p->stylesheet, &n_templates);

	for ( i=0; i<n_templates; i++ ) {
		printf("%2i: %s %s\n", i, templates[i].name,
		       templates[i].friendlyname);
		free(templates[i].name);
		free(templates[i].friendlyname);
		sc_block_free(templates[i].scblock);
	}

	free(templates);
}


static SCBlock *get_slide_template(SCBlock *ss)
{
	struct template_id *templates;
	int i, n_templates;
	SCBlock *ret = NULL;

	templates = get_templates(ss, &n_templates);

	for ( i=0; i<n_templates; i++ ) {
		if ( strcmp(templates[i].name, "slide") == 0 ) {
			ret = templates[i].scblock;
		} else {
			sc_block_free(templates[i].scblock);
		}
		free(templates[i].name);
		free(templates[i].friendlyname);
	}
	free(templates);

        /* No template? */
        if ( ret == NULL ) {
		ret = sc_parse("\\slide{}");
	}

	return ret;  /* NB this is a copy of the one owned by the interpreter */
}


static SCBlock **get_ss_list(struct presentation *p)
{
	SCBlock **stylesheets;

	stylesheets = malloc(3 * sizeof(SCBlock *));
	if ( stylesheets == NULL ) return NULL;

	if ( p->stylesheet != NULL ) {
		stylesheets[0] = p->stylesheet;
		stylesheets[1] = NULL;
	} else {
		stylesheets[0] = NULL;
	}

	return stylesheets;
}


static gint load_ss_response_sig(GtkWidget *d, gint response,
                                 NarrativeWindow *nw)
{
	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;
		char *stext;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
		printf("Loading %s\n",filename);

		stext = load_everything(filename);
		if ( stext != NULL ) {
			SCBlock *bl;
			SCBlock *ss;
			bl = sc_parse(stext);
			free(stext);
			ss = find_stylesheet(bl);
			if ( ss != NULL ) {

				SCBlock **stylesheets;

				/* Substitute the style sheet */
				replace_stylesheet(nw->p, ss);

				stylesheets = get_ss_list(nw->p);
				sc_editor_set_stylesheets(nw->sceditor,
				                          stylesheets);
				free(stylesheets);

				/* Full rerender, first block may have
				 * changed */
				sc_editor_set_scblock(nw->sceditor,
				                      nw->dummy_top);

			} else {
				fprintf(stderr, "Not a style sheet\n");
			}
		} else {
			fprintf(stderr, "Failed to load\n");
		}

		g_free(filename);

	}

	gtk_widget_destroy(d);

	return 0;
}


static void load_ss_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
	//SCBlock *nsblock;
	//SCBlock *templ;
	NarrativeWindow *nw = vp;
	GtkWidget *d;

	d = gtk_file_chooser_dialog_new("Load stylesheet",
	                                GTK_WINDOW(nw->window),
	                                GTK_FILE_CHOOSER_ACTION_OPEN,
	                                "_Cancel", GTK_RESPONSE_CANCEL,
	                                "_Open", GTK_RESPONSE_ACCEPT,
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

	/* Split the current paragraph */
	nsblock = split_paragraph_at_cursor(nw->sceditor);

	/* Get the template */
	templ = get_slide_template(nw->p->stylesheet); /* our copy */
	show_sc_blocks(templ);

	/* Link the new SCBlock in */
	if ( nsblock != NULL ) {
		sc_block_append_p(nsblock, templ);
	} else {
		fprintf(stderr, "Failed to split paragraph\n");
	}

	sc_editor_set_scblock(nw->sceditor,
	                      sc_editor_get_scblock(nw->sceditor));
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
	if ( ss->single_monitor ) {
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
	get_titlebar_string(nw->p);

	if ( nw->p->slidewindow != NULL ) {

		char *title;

		title = malloc(strlen(nw->p->titlebar)+14);
		sprintf(title, "%s - Colloquium", nw->p->titlebar);
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
	SCBlock *stylesheets[2];
	struct frame *top;
	int sn = slide_number(p, scblocks);

	scblocks = sc_block_child(scblocks);
	stylesheets[0] = p->stylesheet;
	stylesheets[1] = NULL;

	/* FIXME: Cache like crazy here */
	surf = render_sc(scblocks, w, h, p->slide_width, p->slide_height,
	                 stylesheets, NULL, p->is, sn, &top, p->lang);
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

	{ "save", save_sig, NULL, NULL, NULL },
	{ "saveas", saveas_sig, NULL, NULL, NULL },
	{ "sorter", open_slidesorter_sig, NULL, NULL, NULL },
	{ "deleteslide", delete_slide_sig, NULL, NULL, NULL },
	{ "slide", add_slide_sig, NULL, NULL, NULL },
	{ "loadstylesheet", load_ss_sig, NULL, NULL, NULL },
	{ "startslideshow", start_slideshow_sig, NULL, NULL, NULL },
	{ "startslideshowhere", start_slideshow_here_sig, NULL, NULL, NULL },
	{ "startslideshownoslides", start_slideshow_noslides_sig, NULL, NULL, NULL },
	{ "clock", open_clock_sig, NULL, NULL, NULL },
	{ "testcard", testcard_sig, NULL, NULL, NULL },
	{ "first", first_para_sig, NULL, NULL, NULL },
	{ "prev", prev_para_sig, NULL, NULL, NULL },
	{ "next", next_para_sig, NULL, NULL, NULL },
	{ "last", last_para_sig, NULL, NULL, NULL },
};


GActionEntry nw_entries_p[] = {
	{ "print", print_sig, NULL, NULL, NULL  },
	{ "exportpdf", exportpdf_sig, NULL, NULL, NULL  },
};


void update_titlebar(NarrativeWindow *nw)
{
	char *title;

	title = get_titlebar_string(nw->p);
	title = realloc(title, strlen(title)+16);
	if ( title == NULL ) return;

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
	SCBlock **stylesheets;
	SCCallbackList *cbl;
	GtkWidget *image;
	Colloquium *app = COLLOQUIUM(papp);

	if ( p->narrative_window != NULL ) {
		fprintf(stderr, "Narrative window is already open!\n");
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
	g_action_map_add_action_entries(G_ACTION_MAP(nw->window), nw_entries_p,
	                                G_N_ELEMENTS(nw_entries_p), p);

	nw_update_titlebar(nw);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(nw->window), vbox);

	stylesheets = get_ss_list(p);

	/* If the presentation is completely empty, give ourselves at least
	 * something to work with */
	if ( nw->p->scblocks == NULL ) {
		nw->p->scblocks = sc_parse("");
	}

	/* Put everything we have inside \presentation{}.
	 * SCEditor will start processing one level down */
	nw->dummy_top = sc_block_new_parent(nw->p->scblocks, "presentation");

	nw->sceditor = sc_editor_new(nw->dummy_top, stylesheets, p->lang,
	                             colloquium_get_imagestore(app));
	free(stylesheets);
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
	button = gtk_tool_button_new(image, "Start slideshow");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.startslideshow");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	/* Add slide */
	image = gtk_image_new_from_icon_name("list-add",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	button = gtk_tool_button_new(image, "Add slide");
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
	                               "win.slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	button = gtk_separator_tool_item_new();
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(button));

	image = gtk_image_new_from_icon_name("go-top",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bfirst = gtk_tool_button_new(image, "First slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bfirst));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bfirst),
	                               "win.first");

	image = gtk_image_new_from_icon_name("go-up",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bprev = gtk_tool_button_new(image, "Previous slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bprev));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bprev),
	                               "win.prev");

	image = gtk_image_new_from_icon_name("go-down",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->bnext = gtk_tool_button_new(image, "Next slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->bnext));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bnext),
	                               "win.next");

	image = gtk_image_new_from_icon_name("go-bottom",
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
	nw->blast = gtk_tool_button_new(image, "Last slide");
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(nw->blast));
	gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->blast),
	                               "win.last");

	update_toolbar(nw);
	update_template_menus(nw);

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
