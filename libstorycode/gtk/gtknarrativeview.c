/*
 * gtknarrativeview.c
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

#include <presentation.h>
#include <narrative_render_cairo.h>

//#include "slide_window.h"
#include "gtknarrativeview.h"
#include "narrative_priv.h"
//#include "slideshow.h"


static void scroll_interface_init(GtkScrollable *iface)
{
}


enum
{
	GTKNARRATIVEVIEW_0,
	GTKNARRATIVEVIEW_VADJ,
	GTKNARRATIVEVIEW_HADJ,
	GTKNARRATIVEVIEW_VPOL,
	GTKNARRATIVEVIEW_HPOL,
};


G_DEFINE_TYPE_WITH_CODE(GtkNarrativeView, gtk_narrative_view,
                        GTK_TYPE_DRAWING_AREA,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE,
                                              scroll_interface_init))

static void redraw(GtkNarrativeView *e)
{
	gint w, h;
	w = gtk_widget_get_allocated_width(GTK_WIDGET(e));
	h = gtk_widget_get_allocated_height(GTK_WIDGET(e));
	gtk_widget_queue_draw_area(GTK_WIDGET(e), 0, 0, w, h);
}


static void horizontal_adjust(GtkAdjustment *adj, GtkNarrativeView *e)
{
	e->h_scroll_pos = gtk_adjustment_get_value(adj);
	redraw(e);
}


static void set_horizontal_params(GtkNarrativeView *e)
{
	if ( e->hadj == NULL ) return;
	gtk_adjustment_configure(e->hadj, e->h_scroll_pos, 0, e->w, 100,
	                         e->visible_width, e->visible_width);
}


static void vertical_adjust(GtkAdjustment *adj, GtkNarrativeView *e)
{
	e->scroll_pos = gtk_adjustment_get_value(adj);
	redraw(e);
}


static void set_vertical_params(GtkNarrativeView *e)
{
	double page;

	if ( e->vadj == NULL ) return;

	/* Ensure we do not scroll off the top of the document */
	if ( e->scroll_pos < 0.0 ) e->scroll_pos = 0.0;

	/* Ensure we do not scroll off the bottom of the document */
	if ( e->scroll_pos > e->h - e->visible_height ) {
		e->scroll_pos = e->h - e->visible_height;
	}

	/* If we can show the whole document, show it at the top */
	if ( e->h < e->visible_height ) {
		e->scroll_pos = 0.0;
	}

	if ( e->h > e->visible_height ) {
		page = e->visible_height;
	} else {
		page = e->h;
	}

	gtk_adjustment_configure(e->vadj, e->scroll_pos, 0, e->h, 100,
	                         e->visible_height, page);
}


static void rewrap_range(GtkNarrativeView *e, int min, int max)
{
	PangoContext *pc;
	PangoLanguage *lang;
	const char *langname;

	pc = gtk_widget_get_pango_context(GTK_WIDGET(e));

	langname = presentation_get_language(e->p);
	lang = pango_language_from_string(langname);

	/* Wrap everything with the current width, to get the total height */
	narrative_wrap_range(presentation_get_narrative(e->p),
	                     presentation_get_stylesheet(e->p),
	                     lang, pc, e->w,
	                     presentation_get_imagestore(e->p),
	                     min, max, e->sel_start, e->sel_end);
}


static void update_size(GtkNarrativeView *e)
{
	e->w = e->visible_width;
	e->h = narrative_get_height(presentation_get_narrative(e->p));

	set_vertical_params(e);
	set_horizontal_params(e);
}


static gboolean resize_sig(GtkWidget *widget, GdkEventConfigure *event,
                           GtkNarrativeView *e)
{
	Narrative *n;

	n = presentation_get_narrative(e->p);

	e->visible_height = event->height;
	e->visible_width = event->width;
	e->w = e->visible_width;

	rewrap_range(e, 0, n->n_items-1);

	update_size(e);

	return FALSE;
}


static void emit_change_sig(GtkNarrativeView *e)
{
	g_signal_emit_by_name(e, "changed");
}


static void sc_editor_set_property(GObject *obj, guint id, const GValue *val,
                                   GParamSpec *spec)
{
	GtkNarrativeView *e = GTK_NARRATIVE_VIEW(obj);

	switch ( id ) {

		case GTKNARRATIVEVIEW_VPOL :
		e->vpol = g_value_get_enum(val);
		break;

		case GTKNARRATIVEVIEW_HPOL :
		e->hpol = g_value_get_enum(val);
		break;

		case GTKNARRATIVEVIEW_VADJ :
		e->vadj = g_value_get_object(val);
		set_vertical_params(e);
		if ( e->vadj != NULL ) {
			g_signal_connect(G_OBJECT(e->vadj), "value-changed",
			                 G_CALLBACK(vertical_adjust), e);
		}
		break;

		case GTKNARRATIVEVIEW_HADJ :
		e->hadj = g_value_get_object(val);
		set_horizontal_params(e);
		if ( e->hadj != NULL ) {
			g_signal_connect(G_OBJECT(e->hadj), "value-changed",
			                 G_CALLBACK(horizontal_adjust), e);
		}
		break;

		default :
		printf("setting %i\n", id);
		break;

	}
}


static void sc_editor_get_property(GObject *obj, guint id, GValue *val,
                                   GParamSpec *spec)
{
	GtkNarrativeView *e = GTK_NARRATIVE_VIEW(obj);

	switch ( id ) {

		case GTKNARRATIVEVIEW_VADJ :
		g_value_set_object(val, e->vadj);
		break;

		case GTKNARRATIVEVIEW_HADJ :
		g_value_set_object(val, e->hadj);
		break;

		case GTKNARRATIVEVIEW_VPOL :
		g_value_set_enum(val, e->vpol);
		break;

		case GTKNARRATIVEVIEW_HPOL :
		g_value_set_enum(val, e->hpol);
		break;

		default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;

	}
}


static GtkSizeRequestMode get_request_mode(GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}


static void get_preferred_width(GtkWidget *widget, gint *min, gint *natural)
{
	*min = 100;
	*natural = 640;
}


static void get_preferred_height(GtkWidget *widget, gint *min, gint *natural)
{
	*min = 1000;
	*natural = 1000;
}


static void gtk_narrative_view_class_init(GtkNarrativeViewClass *klass)
{
	GObjectClass *goc = G_OBJECT_CLASS(klass);
	goc->set_property = sc_editor_set_property;
	goc->get_property = sc_editor_get_property;
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_VADJ, "vadjustment");
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_HADJ, "hadjustment");
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_VPOL, "vscroll-policy");
	g_object_class_override_property(goc, GTKNARRATIVEVIEW_HPOL, "hscroll-policy");

	GTK_WIDGET_CLASS(klass)->get_request_mode = get_request_mode;
	GTK_WIDGET_CLASS(klass)->get_preferred_width = get_preferred_width;
	GTK_WIDGET_CLASS(klass)->get_preferred_height = get_preferred_height;
	GTK_WIDGET_CLASS(klass)->get_preferred_height_for_width = NULL;

	g_signal_new("changed", GTK_TYPE_NARRATIVE_VIEW,
	             G_SIGNAL_RUN_LAST, 0,
	             NULL, NULL, NULL, G_TYPE_NONE, 0);
	g_signal_new("slide-double-clicked", GTK_TYPE_NARRATIVE_VIEW,
	             G_SIGNAL_RUN_LAST, 0,
	             NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
}


static void gtk_narrative_view_init(GtkNarrativeView *e)
{
	e->vpol = GTK_SCROLL_NATURAL;
	e->hpol = GTK_SCROLL_NATURAL;
	e->vadj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
	e->hadj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
}


static void paste_storycode_received(GtkClipboard *cb, GtkSelectionData *seldata,
                                     gpointer vp)
{
//	GtkNarrativeView *e = vp;
//	SCBlock *nf;
//	const guchar *t;
//
//	t = gtk_selection_data_get_data(seldata);
//
//	printf("received storycode paste\n");
//	printf("'%s'\n", t);
//	if ( t == NULL ) return;
//
//	/* FIXME: It might not be a new frame */
//	nf = sc_parse((char *)t);
//	show_sc_blocks(nf);
//	sc_block_append_block(sc_block_child(e->scblocks), nf);
//	full_rerender(e);
}


static void paste_text_received(GtkClipboard *cb, GtkSelectionData *seldata,
                                gpointer vp)
{
//	GtkNarrativeView *e = vp;
//	SCBlock *bl;
//	guchar *t;
//	SCBlock *cur_bl;
//	size_t cur_sc_pos;
//	size_t offs;
//	Paragraph *para;
//
//	t = gtk_selection_data_get_text(seldata);
//
//	printf("received text paste\n");
//	printf("'%s'\n", t);
//	if ( t == NULL ) return;
//
//	bl = sc_parse((char *)t);
//
//	if ( e->cursor_frame == NULL ) {
//		fprintf(stderr, _("No frame selected for paste\n"));
//		return;
//	}
//
//	para = e->cursor_frame->paras[e->cpos.para];
//	offs = pos_trail_to_offset(para, e->cpos.pos, e->cpos.trail);
//
//	get_sc_pos(e->cursor_frame, e->cpos.para, offs, &cur_bl, &cur_sc_pos);
//	sc_insert_block(cur_bl, cur_sc_pos, bl);
//	full_rerender(e);
}


static void paste_targets_received(GtkClipboard *cb, GdkAtom *targets,
                                   gint n_targets, gpointer vp)
{
	GtkNarrativeView *e = vp;
	int i;
	int have_sc = 0;
	int index_sc, index_text;
	int have_text = 0;

	if ( targets == NULL ) {
		fprintf(stderr, "No paste targets offered.\n");
		return;
	}

	for ( i=0; i<n_targets; i++ ) {
		gchar *name = gdk_atom_name(targets[i]);
		if ( g_strcmp0(name, "text/x-storycode") == 0 ) {
			have_sc = 1;
			index_sc = i;
		}
		if ( g_strcmp0(name, "text/plain") == 0 ) {
			have_text = 1;
			index_text = i;
		}
		g_free(name);
	}

	if ( have_sc ) {
		printf("storycode is offered\n");
		gtk_clipboard_request_contents(cb, targets[index_sc],
		                               paste_storycode_received, e);
	} else if ( have_text ) {
		printf("text is offered\n");
		gtk_clipboard_request_contents(cb, targets[index_text],
		                               paste_text_received, e);
	} else {
		printf("nothing useful is offered\n");
	}
}


void sc_editor_paste(GtkNarrativeView *e)
{
	GtkClipboard *cb;
	GdkAtom atom;

	printf("pasting\n");

	atom = gdk_atom_intern("CLIPBOARD", FALSE);
	if ( atom == GDK_NONE ) return;
	cb = gtk_clipboard_get(atom);
	gtk_clipboard_request_targets(cb, paste_targets_received, e);
}


static void clipboard_get(GtkClipboard *cb, GtkSelectionData *seldata,
                          guint info, gpointer data)
{
	char *t = data;

	printf("clipboard get\n");

	if ( info == 0 ) {
		printf("sending SC frame\n");
		gtk_selection_data_set(seldata,
		                       gtk_selection_data_get_target(seldata),
		                       8, (const guchar *)t, strlen(t)+1);
	} else {
		GdkAtom target;
		gchar *name;
		target = gtk_selection_data_get_target(seldata);
		name = gdk_atom_name(target);
		fprintf(stderr, "Don't know what to send for %s\n", name);
		g_free(name);
	}
}


static void clipboard_clear(GtkClipboard *cb, gpointer data)
{
	free(data);
}


static void copy_selection(GtkNarrativeView *e)
{
//	char *t;
//	GtkClipboard *cb;
//	GdkAtom atom;
//	GtkTargetEntry targets[1];
//
//	atom = gdk_atom_intern("CLIPBOARD", FALSE);
//	if ( atom == GDK_NONE ) return;
//
//	cb = gtk_clipboard_get(atom);
//
//	targets[0].target = "text/x-storycode";
//	targets[0].flags = 0;
//	targets[0].info = 0;
//
//	printf("copying selection\n");
//
//	bl = block_at_cursor(e->cursor_frame, e->cpos.para, 0);
//	if ( bl == NULL ) return;
//
//	t = serialise_sc_block(bl);
//
//	gtk_clipboard_set_with_data(cb, targets, 1,
//	                            clipboard_get, clipboard_clear, t);
}


static gint destroy_sig(GtkWidget *window, GtkNarrativeView *e)
{
	return 0;
}


static double para_top(Narrative *n, int pnum)
{
	int i;
	double py = 0.0;
	for ( i=0; i<pnum; i++ ) py += n->items[i].h;
	return py + n->items[pnum].space_t;
}


static void draw_para_highlight(cairo_t *cr, Narrative *n, int cursor_para)
{
	double cx, cy, cw, ch;

	cx = n->space_l;
	cy = n->space_t + para_top(n, cursor_para);
	cw = n->items[cursor_para].slide_w;
	ch = n->items[cursor_para].slide_h;

	cairo_new_path(cr);
	cairo_rectangle(cr, cx, cy, cw, ch);
	cairo_set_source_rgba(cr, 0.7, 0.7, 1.0, 0.5);
	cairo_set_line_width(cr, 5.0);
	cairo_stroke(cr);
}


static size_t pos_trail_to_offset(struct narrative_item *item, int offs, int trail)
{
	glong char_offs;
	char *ptr;

	if ( item->type == NARRATIVE_ITEM_SLIDE ) {
		return offs;
	}

	char_offs = g_utf8_pointer_to_offset(item->text, item->text+offs);
	char_offs += trail;
	ptr = g_utf8_offset_to_pointer(item->text, char_offs);
	return ptr - item->text;
}


static void get_cursor_pos(Narrative *n, struct edit_pos cpos,
                           double *x, double *y, double *h)
{
	size_t offs;
	PangoRectangle rect;
	struct narrative_item *item;

	item = &n->items[cpos.para];
	if ( item->type == NARRATIVE_ITEM_SLIDE ) {
		*x = n->space_l + item->space_l;
		*y = n->space_t + para_top(n, cpos.para);
		*h = item->slide_h;
		return;
	}

	if ( item->layout == NULL ) {
		fprintf(stderr, "get_cursor_pos: No layout\n");
		return;
	}

	offs = pos_trail_to_offset(item, cpos.pos, cpos.trail);
	pango_layout_get_cursor_pos(item->layout, offs, &rect, NULL);
	*x = pango_units_to_double(rect.x) + n->space_l + item->space_l;
	*y = pango_units_to_double(rect.y) + n->space_t + para_top(n, cpos.para);
	*h = pango_units_to_double(rect.height);
}


static void draw_caret(cairo_t *cr, Narrative *n, struct edit_pos cpos,
                       int hgh)
{
	double cx, clow, chigh, h;
	const double t = 1.8;

	if ( hgh ) {
		draw_para_highlight(cr, n, cpos.para);
		return;
	}

	assert(n != NULL);

	if ( n->items[cpos.para].type == NARRATIVE_ITEM_SLIDE ) {
		draw_para_highlight(cr, n, cpos.para);
		return;
	}

	get_cursor_pos(n, cpos, &cx, &clow, &h);

	chigh = clow+h;

	cairo_move_to(cr, cx, clow);
	cairo_line_to(cr, cx, chigh);

	cairo_move_to(cr, cx-t, clow-t);
	cairo_line_to(cr, cx, clow);
	cairo_move_to(cr, cx+t, clow-t);
	cairo_line_to(cr, cx, clow);

	cairo_move_to(cr, cx-t, chigh+t);
	cairo_line_to(cr, cx, chigh);
	cairo_move_to(cr, cx+t, chigh+t);
	cairo_line_to(cr, cx, chigh);

	cairo_set_source_rgb(cr, 0.86, 0.0, 0.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);
}


static void draw_overlay(cairo_t *cr, GtkNarrativeView *e)
{
	draw_caret(cr, presentation_get_narrative(e->p),
	           e->cpos, e->para_highlight);
}


static gboolean draw_sig(GtkWidget *da, cairo_t *cr, GtkNarrativeView *e)
{
	/* Ultimate background */
	cairo_set_source_rgba(cr, 0.8, 0.8, 1.0, 1.0);
	cairo_paint(cr);

	cairo_translate(cr, -e->h_scroll_pos, -e->scroll_pos);

	/* Rendering background */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_rectangle(cr, 0.0, 0.0, e->w, e->h);
	cairo_fill(cr);

	/* Contents */
	narrative_render_cairo(presentation_get_narrative(e->p), cr,
	                       presentation_get_stylesheet(e->p));

	/* Editing overlay */
	draw_overlay(cr, e);

	return FALSE;
}


static void check_cursor_visible(GtkNarrativeView *e)
{
	Narrative *n;
	double x, y, h;

	n = presentation_get_narrative(e->p);
	get_cursor_pos(n, e->cpos, &x, &y, &h);

	/* Off the bottom? */
	if ( y - e->scroll_pos + h > e->visible_height ) {
		e->scroll_pos = y + h - e->visible_height;
		e->scroll_pos += n->space_b;
	}

	/* Off the top? */
	if ( y < e->scroll_pos ) {
		e->scroll_pos = y - n->space_t;
	}
}


static struct narrative_item *get_current_item(GtkNarrativeView *e,
                                               Narrative **pn)
{
	Narrative *n = presentation_get_narrative(e->p);
	if ( pn != NULL ) *pn = n;
	return &n->items[e->cpos.para];
}


static size_t end_offset_of_para(Narrative *n, int pnum)
{
	assert(pnum >= 0);
	if ( n->items[pnum].type == NARRATIVE_ITEM_SLIDE ) return 0;
	return strlen(n->items[pnum].text);
}


static void sort_positions(struct edit_pos *a, struct edit_pos *b)
{
	if ( a->para > b->para ) {
		size_t tpos;
		int tpara, ttrail;
		tpara = b->para;   tpos = b->pos;  ttrail = b->trail;
		b->para = a->para;  b->pos = a->pos;  b->trail = a->trail;
		a->para = tpara;    a->pos = tpos;    a->trail = ttrail;
	}

	if ( (a->para == b->para) && (a->pos > b->pos) )
	{
		size_t tpos = b->pos;
		int ttrail = b->trail;
		b->pos = a->pos;  b->trail = a->trail;
		a->pos = tpos;    a->trail = ttrail;
	}
}


static void cursor_moveh(Narrative *n, struct edit_pos *cp, signed int dir)
{
	struct narrative_item *item = &n->items[cp->para];
	int np = cp->pos;

	if ( item->type == NARRATIVE_ITEM_SLIDE ) {
		if ( dir > 0 ) {
			np = G_MAXINT;
			cp->trail = 0;
		} else {
			np = -1;
			cp->trail = 0;
		}
	} else {
		if ( item->layout == NULL ) return;
		pango_layout_move_cursor_visually(item->layout, 1, cp->pos,
		                                  cp->trail, dir,
		                                  &np, &cp->trail);
	}

	if ( np == -1 ) {
		if ( cp->para > 0 ) {
			size_t end_offs;
			cp->para--;
			end_offs = end_offset_of_para(n, cp->para);
			if ( end_offs > 0 ) {
				cp->pos = end_offs - 1;
				cp->trail = 1;
			} else {
				/* Jumping into an empty paragraph */
				cp->pos = 0;
				cp->trail = 0;
			}
			return;
		} else {
			/* Can't move any further */
			return;
		}
	}

	if ( np == G_MAXINT ) {
		if ( cp->para < n->n_items-1 ) {
			cp->para++;
			cp->pos = 0;
			cp->trail = 0;
			return;
		} else {
			/* Can't move any further */
			cp->trail = 1;
			return;
		}
	}

	cp->pos = np;
}


static void unset_selection(GtkNarrativeView *e)
{
	int a, b;

	a = e->sel_start.para;
	b = e->sel_end.para;
	if ( a > b ) {
		a = e->sel_end.para;
		b = e->sel_start.para;
	}
	e->sel_start.para = 0;
	e->sel_start.pos = 0;
	e->sel_start.trail = 0;
	e->sel_end.para = 0;
	e->sel_end.pos = 0;
	e->sel_end.trail = 0;
	rewrap_range(e, a, b);
}


static int positions_equal(struct edit_pos a, struct edit_pos b)
{
	if ( a.para != b.para ) return 0;
	if ( a.pos != b.pos ) return 0;
	if ( a.trail != b.trail ) return 0;
	return 1;
}


static void do_backspace(GtkNarrativeView *e, signed int dir)
{
	Narrative *n;
	struct edit_pos p1, p2;
	size_t o1, o2;

	n = presentation_get_narrative(e->p);

	if ( !positions_equal(e->sel_start, e->sel_end) ) {

		/* Block delete */
		p1 = e->sel_start;
		p2 = e->sel_end;

	} else {

		/* Delete one character, as represented visually */
		p2 = e->cpos;
		p1 = p2;
		cursor_moveh(n, &p1, dir);
	}

	sort_positions(&p1, &p2);
	o1 = pos_trail_to_offset(&n->items[p1.para], p1.pos, p1.trail);
	o2 = pos_trail_to_offset(&n->items[p2.para], p2.pos, p2.trail);
	narrative_delete_block(n, p1.para, o1, p2.para, o2);
	e->cpos = p1;
	unset_selection(e);

	/* The only paragraphs which still exist and might have been
	 * affected by the deletion are sel_start.para and the one
	 * immediately afterwards. */
	rewrap_range(e, p1.para, p1.para+1);
	update_size(e);
	emit_change_sig(e);
	redraw(e);
}


static void insert_text_in_paragraph(struct narrative_item *item, size_t offs,
                                     char *t)
{
	char *n = malloc(strlen(t) + strlen(item->text) + 1);
	if ( n == NULL ) return;
	strncpy(n, item->text, offs);
	n[offs] = '\0';
	strcat(n, t);
	strcat(n, item->text+offs);
	free(item->text);
	item->text = n;
}


static void split_paragraph_at_cursor(Narrative *n, struct edit_pos pos)
{
	size_t off;

	if ( n->items[pos.para].type != NARRATIVE_ITEM_SLIDE ) {
		off = pos_trail_to_offset(&n->items[pos.para],
		                          pos.pos, pos.trail);
	} else {
		off = 0;
	}

	narrative_split_item(n, pos.para, off);
}


static void insert_text(char *t, GtkNarrativeView *e)
{
	Narrative *n;
	struct narrative_item *item;

	if ( !positions_equal(e->sel_start, e->sel_end) ) {
		do_backspace(e, 0);
	}

	item = get_current_item(e, &n);

	if ( strcmp(t, "\n") == 0 ) {
		split_paragraph_at_cursor(n, e->cpos);
		rewrap_range(e, e->cpos.para, e->cpos.para+1);
		update_size(e);
		cursor_moveh(n, &e->cpos, +1);
		check_cursor_visible(e);
		emit_change_sig(e);
		redraw(e);
		return;
	}

	if ( item->type != NARRATIVE_ITEM_SLIDE ) {

		size_t off;

		off = pos_trail_to_offset(item, e->cpos.pos, e->cpos.trail);
		insert_text_in_paragraph(item, off, t);
		rewrap_range(e, e->cpos.para, e->cpos.para);
		update_size(e);
		cursor_moveh(n, &e->cpos, +1);

	} /* else do nothing: pressing enter is OK, though */

	emit_change_sig(e);
	check_cursor_visible(e);
	redraw(e);
}


static gboolean im_commit_sig(GtkIMContext *im, gchar *str,
                              GtkNarrativeView *e)
{
	insert_text(str, e);
	return FALSE;
}


static int find_cursor(Narrative *n, double x, double y, struct edit_pos *pos)
{
	double cur_y;
	struct narrative_item *item;
	int i = 0;

	cur_y = n->space_t;

	do {
		cur_y += n->items[i++].h;
	} while ( (cur_y < y) && (i<n->n_items) );

	pos->para = i-1;
	item = &n->items[pos->para];
	if ( item->type == NARRATIVE_ITEM_SLIDE ) {
		pos->pos = 0;
		return 0;
	}

	pango_layout_xy_to_index(item->layout,
	                         pango_units_from_double(x - n->space_l - item->space_l),
	                         pango_units_from_double(y - n->space_t - para_top(n, pos->para)),
	                         &pos->pos, &pos->trail);

	return 0;
}


static gboolean button_press_sig(GtkWidget *da, GdkEventButton *event,
                                 GtkNarrativeView *e)
{
	gdouble x, y;
	Narrative *n;

	n = presentation_get_narrative(e->p);
	x = event->x;
	y = event->y + e->scroll_pos;

	/* Clicked an existing frame, no immediate dragging */
	e->drag_status = DRAG_STATUS_COULD_DRAG;
	unset_selection(e);
	find_cursor(n, x, y, &e->sel_start);
	e->sel_end = e->sel_start;
	e->cpos = e->sel_start;

	if ( event->type == GDK_2BUTTON_PRESS ) {
		struct narrative_item *item = &n->items[e->cpos.para];
		if ( item->type == NARRATIVE_ITEM_SLIDE ) {
			g_signal_emit_by_name(e, "slide-double-clicked",
			                      item->slide);
		}
	}

	gtk_widget_grab_focus(GTK_WIDGET(da));
	redraw(e);
	return FALSE;
}


static void sorti(int *a, int *b)
{
	if ( *a > *b ) {
		int tmp = *a;
		*a = *b;
		*b = tmp;
	}
}


static gboolean motion_sig(GtkWidget *da, GdkEventMotion *event,
                           GtkNarrativeView *e)
{
	Narrative *n;
	gdouble x, y;
	struct edit_pos old_sel_end;
	int minp, maxp;

	x = event->x;
	y = event->y + e->scroll_pos;

	if ( e->drag_status == DRAG_STATUS_COULD_DRAG ) {
		/* We just got a motion signal, and the status was "could drag",
		 * therefore the drag has started. */
		e->drag_status = DRAG_STATUS_DRAGGING;
	}

	old_sel_end = e->sel_end;
	n = presentation_get_narrative(e->p);
	find_cursor(n, x, y, &e->sel_end);

	minp = e->sel_start.para;
	maxp = e->sel_end.para;
	sorti(&minp, &maxp);
	if ( !positions_equal(e->sel_start, old_sel_end) ) {
		if ( old_sel_end.para > maxp ) maxp = old_sel_end.para;
		if ( old_sel_end.para < minp ) minp = old_sel_end.para;
	}

	rewrap_range(e, minp, maxp);
	find_cursor(n, x, y, &e->cpos);
	redraw(e);

	gdk_event_request_motions(event);
	return FALSE;
}


static gboolean key_press_sig(GtkWidget *da, GdkEventKey *event,
                              GtkNarrativeView *e)
{
	gboolean r;
	Narrative *n;
	int claim = 0;

	/* Throw the event to the IM context and let it sort things out */
	r = gtk_im_context_filter_keypress(GTK_IM_CONTEXT(e->im_context),
		                           event);
	if ( r ) return FALSE;  /* IM ate it */

	n = presentation_get_narrative(e->p);

	switch ( event->keyval ) {

		case GDK_KEY_Left :
		cursor_moveh(n, &e->cpos, -1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Right :
		cursor_moveh(n, &e->cpos, +1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Up :
		cursor_moveh(n, &e->cpos, -1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Down :
		cursor_moveh(n, &e->cpos, +1);
		redraw(e);
		claim = 1;
		break;

		case GDK_KEY_Return :
		im_commit_sig(NULL, "\n", e);
		claim = 1;
		break;

		case GDK_KEY_BackSpace :
		do_backspace(e, -1);
		claim = 1;
		break;

		case GDK_KEY_Delete :
		do_backspace(e, +1);
		claim = 1;
		break;

		case GDK_KEY_C :
		case GDK_KEY_c :
		if ( event->state == GDK_CONTROL_MASK ) {
			copy_selection(e);
		}
		break;

		case GDK_KEY_V :
		case GDK_KEY_v :
		if ( event->state == GDK_CONTROL_MASK ) {
			sc_editor_paste(e);
		}
		break;


	}

	if ( claim ) return TRUE;
	return FALSE;
}


static gboolean dnd_motion(GtkWidget *widget, GdkDragContext *drag_context,
                           gint x, gint y, guint time, GtkNarrativeView *e)
{
	return TRUE;
}


static gboolean dnd_drop(GtkWidget *widget, GdkDragContext *drag_context,
                         gint x, gint y, guint time, GtkNarrativeView *e)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target(widget, drag_context, NULL);

	if ( target == GDK_NONE ) {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
	} else {
		gtk_drag_get_data(widget, drag_context, target, time);
	}

	return TRUE;
}


static void dnd_receive(GtkWidget *widget, GdkDragContext *drag_context,
                        gint x, gint y, GtkSelectionData *seldata,
                        guint info, guint time, GtkNarrativeView *e)
{
}


static void dnd_leave(GtkWidget *widget, GdkDragContext *drag_context,
                      guint time, GtkNarrativeView *nview)
{
	nview->drag_status = DRAG_STATUS_NONE;
}


static gint realise_sig(GtkWidget *da, GtkNarrativeView *e)
{
	GdkWindow *win;

	/* Keyboard and input method stuff */
	e->im_context = gtk_im_multicontext_new();
	win = gtk_widget_get_window(GTK_WIDGET(e));
	gtk_im_context_set_client_window(GTK_IM_CONTEXT(e->im_context), win);
	gdk_window_set_accept_focus(win, TRUE);
	g_signal_connect(G_OBJECT(e->im_context), "commit", G_CALLBACK(im_commit_sig), e);
	g_signal_connect(G_OBJECT(e), "key-press-event", G_CALLBACK(key_press_sig), e);

	return FALSE;
}


GtkWidget *gtk_narrative_view_new(Presentation *p)
{
	GtkNarrativeView *nview;
	GtkTargetEntry targets[1];

	nview = g_object_new(GTK_TYPE_NARRATIVE_VIEW, NULL);

	nview->w = 100;
	nview->h = 100;
	nview->scroll_pos = 0;
	nview->p = p;

	nview->para_highlight = 0;

	gtk_widget_set_size_request(GTK_WIDGET(nview),
	                            nview->w, nview->h);

	g_signal_connect(G_OBJECT(nview), "destroy",
	                 G_CALLBACK(destroy_sig), nview);
	g_signal_connect(G_OBJECT(nview), "realize",
	                 G_CALLBACK(realise_sig), nview);
	g_signal_connect(G_OBJECT(nview), "button-press-event",
	                 G_CALLBACK(button_press_sig), nview);
	g_signal_connect(G_OBJECT(nview), "motion-notify-event",
	                 G_CALLBACK(motion_sig), nview);
	g_signal_connect(G_OBJECT(nview), "configure-event",
	                 G_CALLBACK(resize_sig), nview);

	/* Drag and drop */
	targets[0].target = "text/uri-list";
	targets[0].flags = 0;
	targets[0].info = 1;
	gtk_drag_dest_set(GTK_WIDGET(nview), 0, targets, 1,
	                  GDK_ACTION_PRIVATE);
	g_signal_connect(nview, "drag-data-received",
	                 G_CALLBACK(dnd_receive), nview);
	g_signal_connect(nview, "drag-motion",
	                 G_CALLBACK(dnd_motion), nview);
	g_signal_connect(nview, "drag-drop",
	                 G_CALLBACK(dnd_drop), nview);
	g_signal_connect(nview, "drag-leave",
	                 G_CALLBACK(dnd_leave), nview);

	gtk_widget_set_can_focus(GTK_WIDGET(nview), TRUE);
	gtk_widget_add_events(GTK_WIDGET(nview),
	                      GDK_POINTER_MOTION_HINT_MASK
	                       | GDK_BUTTON1_MOTION_MASK
	                       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	                       | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK
	                       | GDK_SCROLL_MASK);

	g_signal_connect(G_OBJECT(nview), "draw",
			 G_CALLBACK(draw_sig), nview);

	gtk_widget_grab_focus(GTK_WIDGET(nview));

	gtk_widget_show(GTK_WIDGET(nview));

	return GTK_WIDGET(nview);
}
