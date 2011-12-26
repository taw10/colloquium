/*
 * notes.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2011 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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


static void grab_notes(struct notes *n, struct slide *s)
{
	gchar *text;
	GtkTextBuffer *tb;
	GtkTextIter i1, i2;

	if ( n == NULL ) return;

	tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(n->v));
	gtk_text_buffer_get_start_iter(tb, &i1);
	gtk_text_buffer_get_end_iter(tb, &i2);
	text = gtk_text_buffer_get_text(tb, &i1, &i2, TRUE);

	free(s->notes);
	s->notes = text;
}


void grab_current_notes(struct presentation *p)
{
	grab_notes(p->notes, p->cur_notes_slide);
}


void notify_notes_slide_changed(struct presentation *p, struct slide *np)
{
	grab_notes(p->notes, p->cur_notes_slide);
	p->cur_notes_slide = np;
	update_notes(p);
}


static gint close_notes_sig(GtkWidget *w, struct presentation *p)
{
	grab_notes(p->notes, p->cur_notes_slide);
	p->notes = NULL;
	return FALSE;
}


static char *escape_text(const char *a)
{
	char *b;
	size_t l1, l, i;

	l1 = strlen(a);

	b = malloc(2*l1 + 1);
	l = 0;

	for ( i=0; i<l1; i++ ) {

		char c = a[i];

		/* Yes, this is horribly confusing */
		if ( c == '\n' ) {
			b[l++] = '\\';  b[l++] = 'n';
		} else if ( c == '\r' ) {
			b[l++] = '\\';  b[l++] = 'r';
		} else if ( c == '\"' ) {
			b[l++] = '\\';  b[l++] = '\"';
		} else if ( c == '\t' ) {
			b[l++] = '\\';  b[l++] = 't';
		} else {
			b[l++] = c;
		}

	}
	b[l++] = '\0';

	return realloc(b, l);
}


static char *unescape_text(const char *a)
{
	char *b;
	size_t l1, l, i;
	int escape;

	l1 = strlen(a);

	b = malloc(l1 + 1);
	l = 0;
	escape = 0;

	for ( i=0; i<l1; i++ ) {

		char c = a[i];

		if ( escape ) {
			if ( c == 'r' ) b[l++] = '\r';
			if ( c == 'n' ) b[l++] = '\n';
			if ( c == '\"' ) b[l++] = '\"';
			if ( c == 't' ) b[l++] = '\t';
			escape = 0;
			continue;
		}

		if ( c == '\\' ) {
			escape = 1;
			continue;
		}

		b[l++] = c;

	}
	b[l++] = '\0';

	return realloc(b, l);
}


void write_notes(struct slide *s, struct serializer *ser)
{
	char *es;

	es = escape_text(s->notes);
	serialize_s(ser, "notes", es);
	free(es);
}


void load_notes(struct ds_node *node, struct slide *s)
{
	char *v;

	if ( get_field_s(node, "notes", &v) ) return;

	s->notes = unescape_text(v);
	free(v);
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

	p->cur_notes_slide = p->cur_edit_slide;

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
	gtk_container_add(GTK_CONTAINER(sc), n->v);

	g_signal_connect(G_OBJECT(n->v), "destroy",
	                 G_CALLBACK(close_notes_sig), p);

	set_notes_title(p);
	gtk_widget_show_all(n->window);

	update_notes(p);
}
