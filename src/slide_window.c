/*
 * slide_window.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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

#include <libintl.h>
#define _(x) gettext(x)

#include <narrative.h>
#include <slide.h>
#include <gtkslideview.h>

#include "colloquium.h"
#include "slide_window.h"
#include "narrative_window.h"


struct _slidewindow
{
	GtkWidget           *window;
	Narrative           *n;
	Slide               *slide;
	GtkWidget           *sv;
	NarrativeWindow     *parent;
};


static void insert_slidetitle_sig(GSimpleAction *action, GVariant *parameter,
                                  gpointer vp)
{
	SlideWindow *sw = vp;
	struct text_run *runs;
	int nruns = 1;

	/* Ownership of this struct will be taken over by the Slide. */
	runs = malloc(sizeof(struct text_run));
	runs[0].type = TEXT_RUN_NORMAL;
	runs[0].text = strdup("Slide title");

	slide_add_slidetitle(sw->slide, &runs, &nruns, 1);
	gtk_slide_view_set_slide(sw->sv, sw->slide);
}


static gint insert_image_response_sig(GtkWidget *d, gint response, SlideWindow *sw)
{
	GtkWidget *cb;
	const char *size_str;

	cb = gtk_file_chooser_get_extra_widget(GTK_FILE_CHOOSER(d));
	size_str = gtk_combo_box_get_active_id(GTK_COMBO_BOX(cb));

	if ( response == GTK_RESPONSE_ACCEPT ) {

		char *filename;
		struct frame_geom geom;
		char *fn;
		double slide_w, slide_h;
		gint image_w, image_h;
		double aspect;
		GdkPixbufFormat *f;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
		fn = strdup(filename);
		if ( fn == NULL ) return 0;

		if ( slide_get_logical_size(sw->slide, narrative_get_stylesheet(sw->n),
		                            &slide_w, &slide_h) ) return 0;

		f = gdk_pixbuf_get_file_info(filename, &image_w, &image_h);
		if ( f == NULL ) return 0;
		aspect = (double)image_h / image_w;
		g_free(filename);

		if ( strcmp(size_str, "normal") == 0 ) {
			geom.x.len = slide_w/4.0;  geom.x.unit = LENGTH_UNIT;
			geom.y.len = slide_h/4.0;  geom.y.unit = LENGTH_UNIT;
			geom.w.len = slide_w/2.0;  geom.w.unit = LENGTH_UNIT;
			geom.h.len = geom.w.len*aspect;  geom.h.unit = LENGTH_UNIT;
		}

		if ( strcmp(size_str, "fillentire") == 0 ) {
			geom.x.len = 0.0;  geom.x.unit = LENGTH_UNIT;
			geom.y.len = 0.0;  geom.y.unit = LENGTH_UNIT;
			geom.w.len = 1.0;  geom.w.unit = LENGTH_FRAC;
			geom.h.len = 1.0;  geom.h.unit = LENGTH_FRAC;
		}

		slide_add_image(sw->slide, fn, geom);
	}

	gtk_widget_destroy(d);

	return 0;
}


static void insert_image_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	SlideWindow *sw = vp;
	GtkWidget *d;
	GtkWidget *cb;

	d = gtk_file_chooser_dialog_new(_("Insert image"),
	                                NULL,
	                                GTK_FILE_CHOOSER_ACTION_OPEN,
	                                _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                _("_Insert"), GTK_RESPONSE_ACCEPT,
	                                NULL);

	cb = gtk_combo_box_text_new();

	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "normal",
	                          _("Make the image about half the width of the slide"));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "fillentire",
	                          _("Fill the entire slide, even the title and footer regions"));

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(cb), "normal");

	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(d), cb);

	g_signal_connect(G_OBJECT(d), "response",
	                 G_CALLBACK(insert_image_response_sig), sw);

	gtk_widget_show_all(d);
}


static void paste_sig(GSimpleAction *action, GVariant *parameter,
                      gpointer vp)
{
	//SlideWindow *sw = vp;
	//sc_editor_paste(sw->sceditor);
}


static void copy_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	//SlideWindow *sw = vp;
	//sc_editor_copy_selected_frame(sw->sceditor);
}


static void delete_frame_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
	SlideWindow *sw = vp;
	gtk_slide_view_delete_selected_frame(GTK_SLIDE_VIEW(sw->sv));
}


/* Change the editor's slide to "np" */
static void change_edit_slide(SlideWindow *sw, Slide *np)
{
	gtk_slide_view_set_slide(sw->sv, np);
	sw->slide = np;
	slide_window_update_titlebar(sw);
}


static void change_slide_first(SlideWindow *sw)
{
	Slide *s = narrative_get_slide_by_number(sw->n, 0);
	if ( s != NULL ) change_edit_slide(sw, s);
}


static void change_slide_backwards(SlideWindow *sw)
{
	int slide_n = narrative_get_slide_number_for_slide(sw->n, sw->slide);
	if ( slide_n > 0 ) {
		Slide *s = narrative_get_slide_by_number(sw->n, slide_n-1);
		change_edit_slide(sw, s);
	}
}


static void change_slide_forwards(SlideWindow *sw)
{
	int slide_n = narrative_get_slide_number_for_slide(sw->n, sw->slide);
	Slide *s = narrative_get_slide_by_number(sw->n, slide_n+1);
	if ( s != NULL ) change_edit_slide(sw, s);
}


static void change_slide_last(SlideWindow *sw)
{
	int slide_n = narrative_get_num_slides(sw->n);
	Slide *s = narrative_get_slide_by_number(sw->n, slide_n);
	if ( s != NULL ) change_edit_slide(sw, s);
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


static gboolean sw_close_sig(GtkWidget *w, SlideWindow *sw)
{
	narrative_window_sw_closed(sw->parent, sw);
	return FALSE;
}


static gboolean sw_key_press_sig(GtkWidget *da, GdkEventKey *event,
                                 SlideWindow *sw)
{
	switch ( event->keyval ) {

		case GDK_KEY_Page_Up :
		change_slide_backwards(sw);
		break;

		case GDK_KEY_Page_Down :
		change_slide_forwards(sw);
		break;

	}

	return FALSE;
}


static void about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	SlideWindow *sw = vp;
	open_about_dialog(sw->window);
}


GActionEntry sw_entries[] = {

	{ "about", about_sig, NULL, NULL, NULL },
	{ "paste", paste_sig, NULL, NULL, NULL },
	{ "copyframe", copy_frame_sig, NULL, NULL, NULL },
	{ "deleteframe", delete_frame_sig, NULL, NULL, NULL },
	{ "first", first_slide_sig, NULL, NULL, NULL },
	{ "prev", prev_slide_sig, NULL, NULL, NULL },
	{ "next", next_slide_sig, NULL, NULL, NULL },
	{ "last", last_slide_sig, NULL, NULL, NULL },
	{ "slidetitle", insert_slidetitle_sig, NULL, NULL, NULL },
	{ "image", insert_image_sig, NULL, NULL, NULL },
};


SlideWindow *slide_window_open(Narrative *n, Slide *slide,
                               NarrativeWindow *nw, GApplication *papp)
{
	GtkWidget *window;
	SlideWindow *sw;
	double w, h;
	Colloquium *app = COLLOQUIUM(papp);

	sw = calloc(1, sizeof(SlideWindow));
	if ( sw == NULL ) return NULL;

	window = gtk_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_role(GTK_WINDOW(window), "slide");
	sw->window = window;
	sw->n = n;
	sw->slide = slide;
	sw->parent = nw;

	slide_window_update_titlebar(sw);

	g_action_map_add_action_entries(G_ACTION_MAP(window), sw_entries,
	                                G_N_ELEMENTS(sw_entries), sw);

	g_signal_connect(G_OBJECT(window), "destroy",
	                 G_CALLBACK(sw_close_sig), sw);

	sw->sv = gtk_slide_view_new(n, slide);

	g_signal_connect(G_OBJECT(sw->sv), "key-press-event",
			 G_CALLBACK(sw_key_press_sig), sw);

	slide_get_logical_size(slide, narrative_get_stylesheet(n), &w, &h);
	gtk_window_set_default_size(GTK_WINDOW(window), w, h);

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(sw->sv));

	gtk_window_set_resizable(GTK_WINDOW(sw->window), TRUE);

	gtk_widget_show_all(window);

	return sw;
}


void slide_window_update(SlideWindow *sw)
{
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(sw->sv));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(sw->sv));
	gtk_widget_queue_draw_area(GTK_WIDGET(sw->sv), 0, 0, w, h);
}


void slide_window_update_titlebar(SlideWindow *sw)
{
	char title[1026];
	char *filename;

	filename = narrative_window_get_filename(sw->parent);
	snprintf(title, 1024, "%s (slide %i) - Colloquium", filename,
	         1+narrative_get_slide_number_for_slide(sw->n, sw->slide));
	if ( narrative_get_unsaved(sw->n) ) strcat(title, " *");

	gtk_window_set_title(GTK_WINDOW(sw->window), title);
}


void slide_window_destroy(SlideWindow *sw)
{
	gtk_widget_destroy(sw->window);
}
