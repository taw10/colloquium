/*
 * notes.c
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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
#include <assert.h>
#include <gtk/gtk.h>

#include "presentation.h"
#include "notes.h"

struct notes
{
	GtkWidget *window;
	GtkWidget *v;

	SCBlock *bl;
	SlideWindow *sw;
};


static SCBlock *find_notes_block(SCBlock *s)
{
	SCBlock *bl = s;
	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);

		if ( (name != NULL) && (strcmp(name, "notes") == 0) ) {
			if ( sc_block_child(bl) == NULL ) {
				sc_block_append_inside(s, NULL, NULL,
				                       strdup(""));
			}
			return bl;
		}

		bl = sc_block_next(bl);

	}

	bl = sc_block_append_end(s, "notes", NULL, NULL);
	sc_block_append_inside(bl, NULL, NULL, strdup(""));
	return bl;
}


static void update_notes(struct notes *notes)
{
	GtkTextBuffer *tb;
	const char *ntext;
	SCBlock *ch;

	if ( notes == NULL ) return;

	ch = sc_block_child(notes->bl);
	if ( ch != NULL ) {
		ntext = sc_block_contents(ch);
	} else {
		ntext = "NOTES ERROR";
	}

	tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes->v));
	gtk_text_buffer_set_text(tb, ntext, -1);
}


void grab_current_notes(struct notes *n)
{
	gchar *text;
	GtkTextBuffer *tb;
	GtkTextIter i1, i2;
	SCBlock *ch;

	if ( n == NULL ) return;

	tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(n->v));
	gtk_text_buffer_get_start_iter(tb, &i1);
	gtk_text_buffer_get_end_iter(tb, &i2);
	text = gtk_text_buffer_get_text(tb, &i1, &i2, TRUE);

	ch = sc_block_child(n->bl);
	if ( ch != NULL ) {
		sc_block_set_contents(ch, text);
	} else {
		fprintf(stderr, "NOTES ERROR\n");
	}
}


void notes_set_slide(struct notes *notes, SCBlock *np)
{
	if ( notes == NULL ) return;
	grab_current_notes(notes);
	notes->bl = find_notes_block(np);
	update_notes(notes);
}


static gint close_notes_sig(GtkWidget *w, struct notes *notes)
{
	grab_current_notes(notes);
	slidewindow_notes_closed(notes->sw);
	return FALSE;
}


struct notes *open_notes(SlideWindow *sw, SCBlock *slide)
{
	struct notes *n;
	GtkWidget *sc;
	PangoFontDescription *desc;

	n = malloc(sizeof(struct notes));
	if ( n == NULL ) return NULL;
	n->sw = sw;

	n->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(n->window), 800, 256);
	sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(n->window), sc);

	n->v = gtk_text_view_new();
	desc = pango_font_description_from_string("Sans 24");
	gtk_widget_override_font(n->v, desc);
	pango_font_description_free(desc);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(n->v), 30);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(n->v), 30);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(n->v), GTK_WRAP_WORD_CHAR);
	gtk_container_add(GTK_CONTAINER(sc), n->v);

	g_signal_connect(G_OBJECT(n->v), "destroy",
	                 G_CALLBACK(close_notes_sig), n);

	gtk_window_set_title(GTK_WINDOW(n->window), "Colloquium notes");
	gtk_widget_show_all(n->window);

	update_notes(n);
	return n;
}

