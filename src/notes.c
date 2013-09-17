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


struct notes
{
	GtkWidget *window;
	GtkWidget *v;

	struct slide *slide;
};


static void set_notes_title(struct presentation *p)
{
	gtk_window_set_title(GTK_WINDOW(p->notes->window), "Colloquium notes");
}


static void update_notes(struct presentation *p)
{
	GtkTextBuffer *tb;

	if ( p->notes == NULL ) return;

	tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(p->notes->v));
	gtk_text_buffer_set_text(tb, p->cur_edit_slide->notes, -1);
}


void grab_current_notes(struct presentation *p)
{
	gchar *text;
	GtkTextBuffer *tb;
	GtkTextIter i1, i2;
	struct notes *n = p->notes;

	if ( n == NULL ) return;

	tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(n->v));
	gtk_text_buffer_get_start_iter(tb, &i1);
	gtk_text_buffer_get_end_iter(tb, &i2);
	text = gtk_text_buffer_get_text(tb, &i1, &i2, TRUE);

	free(n->slide->notes);
	n->slide->notes = text;
}


void notify_notes_slide_changed(struct presentation *p, struct slide *np)
{
	grab_current_notes(p);
	p->notes->slide = np;
	update_notes(p);
}


static gint close_notes_sig(GtkWidget *w, struct presentation *p)
{
	grab_current_notes(p);
	p->notes = NULL;
	return FALSE;
}


void write_notes(struct slide *s, struct serializer *ser)
{
	serialize_s(ser, "notes", s->notes);
}


void load_notes(struct ds_node *node, struct slide *s)
{
	char *v;

	if ( get_field_s(node, "notes", &v) ) return;

	s->notes = v;
}


void open_notes(struct presentation *p)
{
	struct notes *n;
	GtkWidget *sc;
	PangoFontDescription *desc;

	if ( p->notes != NULL ) return;  /* Already open */

	n = malloc(sizeof(struct notes));
	if ( n == NULL ) return;
	p->notes = n;

	p->notes->slide = p->cur_edit_slide;

	n->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(n->window), 800, 256);
	sc = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(n->window), sc);

	n->v = gtk_text_view_new();
	desc = pango_font_description_from_string("Sans 24");
	gtk_widget_modify_font(n->v, desc);
	pango_font_description_free(desc);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(n->v), 30);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(n->v), 30);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(n->v), GTK_WRAP_WORD_CHAR);
	gtk_container_add(GTK_CONTAINER(sc), n->v);

	g_signal_connect(G_OBJECT(n->v), "destroy",
	                 G_CALLBACK(close_notes_sig), p);

	set_notes_title(p);
	gtk_widget_show_all(n->window);

	update_notes(p);
}
