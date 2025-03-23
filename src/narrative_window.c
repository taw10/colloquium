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

#include "narrative.h"
#include "colloquium.h"
#include "narrative_window.h"
#include "slide_window.h"
#include "slide_render_cairo.h"
#include "testcard.h"
#include "pr_clock.h"
#include "slideshow.h"
#include "print.h"
#include "narrative_priv.h"
#include "thumbnailwidget.h"

G_DEFINE_TYPE_WITH_CODE(NarrativeWindow, narrativewindow, GTK_TYPE_APPLICATION_WINDOW, NULL)

typedef struct _nv GtkNarrativeView;
#define GTK_NARRATIVE_VIEW(a) ((GtkNarrativeView *)a)
void gtk_narrative_view_add_slide_at_cursor(GtkNarrativeView *e) {}
void gtk_narrative_view_add_bp_at_cursor(GtkNarrativeView *e) {}
void gtk_narrative_view_add_segend_at_cursor(GtkNarrativeView *e) {}
void gtk_narrative_view_add_eop_at_cursor(GtkNarrativeView *e) {}
void gtk_narrative_view_add_segstart_at_cursor(GtkNarrativeView *e) {}
void gtk_narrative_view_add_prestitle_at_cursor(GtkNarrativeView *e) {}
void gtk_narrative_view_redraw(GtkNarrativeView *e) {}


static int get_cursor_para(GtkTextView *nv)
{
    GtkTextBuffer *buf;
    GtkTextMark *cur;
    GtkTextIter iter;

    buf = gtk_text_view_get_buffer(nv);
    cur = gtk_text_buffer_get_insert(buf);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, cur);

    return gtk_text_iter_get_line(&iter);
}


static int num_items(Narrative *n)
{
    return gtk_text_buffer_get_line_count(n->textbuf);
}


static int num_items_to_eop(Narrative *n)
{
    /* FIXME: Decide on end of presentation mark */
    return gtk_text_buffer_get_line_count(n->textbuf);
}


static void show_error(NarrativeWindow *nw, const char *err)
{
    GtkAlertDialog *mw;
    const char *buttons[2] = {"Close", NULL};
    mw = gtk_alert_dialog_new("%s", err);
    gtk_alert_dialog_set_buttons(mw, buttons);
    gtk_alert_dialog_show(mw, GTK_WINDOW(nw));
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
    if ( !nw->n->saved ) strcat(title, " *");
    free(filename);

    gtk_window_set_title(GTK_WINDOW(nw), title);

    for ( i=0; i<nw->n_slidewindows; i++ ) {
        slide_window_update_titlebar(nw->slidewindows[i]);
    }
}


void slide_window_closed_sig(GtkWidget *sw, NarrativeWindow *nw)
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
}


static void update_toolbar(NarrativeWindow *nw)
{
    int cur_para, n_para;

    if ( nw->show == NULL ) return;

    cur_para = get_cursor_para(GTK_TEXT_VIEW(nw->nv));
    if ( cur_para == 0 ) {
        gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), TRUE);
    }

    n_para = num_items(nw->n);
    if ( cur_para == n_para - 1 ) {
        gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), TRUE);
    }
}


static void saveas_response_sig(GObject *d, GAsyncResult *res, gpointer vp)
{
    NarrativeWindow *nw = vp;
    GFile *file;

    file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(d), res, NULL);
    if ( file == NULL ) return;

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


static void saveas_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    GtkFileDialog *d;
    NarrativeWindow *nw = vp;


    d = gtk_file_dialog_new();

    gtk_file_dialog_set_title(d, _("Save presentation"));
    gtk_file_dialog_set_accept_label(d, _("Save"));

    gtk_file_dialog_save(d, GTK_WINDOW(nw),
                         NULL, saveas_response_sig, nw);
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


static void add_slide_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_narrative_view_add_slide_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
    nw->n->saved = 0;
    update_titlebar(nw);
}

static void add_prestitle_sig(GSimpleAction *action, GVariant *parameter,
                              gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_narrative_view_add_prestitle_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
    nw->n->saved = 0;
    update_titlebar(nw);
}


static void add_bp_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_narrative_view_add_bp_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
    nw->n->saved = 0;
    update_titlebar(nw);
}


static void add_segstart_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_narrative_view_add_segstart_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
    nw->n->saved = 0;
    update_titlebar(nw);
}


static void add_segend_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_narrative_view_add_segend_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
    nw->n->saved = 0;
    update_titlebar(nw);
}


static void add_eop_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_narrative_view_add_eop_at_cursor(GTK_NARRATIVE_VIEW(nw->nv));
    nw->n->saved = 0;
    update_titlebar(nw);
}


static void set_clock_pos(NarrativeWindow *nw)
{
    int pos = get_cursor_para(GTK_TEXT_VIEW(nw->nv));
    int end = num_items_to_eop(nw->n);
    if ( pos >= end ) pos = end-1;
    pr_clock_set_pos(nw->pr_clock, pos, end);
}


static void first_para_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
    NarrativeWindow *nw = vp;
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_BUFFER_ENDS, 0, FALSE);
    set_clock_pos(nw);
    update_toolbar(nw);
}


static void ss_prev_para(SCSlideshow *ss, void *vp)
{
    NarrativeWindow *nw = vp;
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPHS, -1, FALSE);
    set_clock_pos(nw);
    update_toolbar(nw);
}


static void prev_para_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    NarrativeWindow *nw = vp;
    ss_prev_para(nw->show, nw);
}


static GtkTextTag *slide_tag(GtkTextBuffer *buf)
{
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
    return gtk_text_tag_table_lookup(table, "slide");
}


static void ss_next_para(SCSlideshow *ss, void *vp)
{
    NarrativeWindow *nw = vp;
    GtkTextMark *cursor;
    GtkTextIter iter;
    GtkTextChildAnchor *anc;

    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPHS, 1, FALSE);
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPH_ENDS, -1, FALSE);

    /* If we only have one monitor, skip to next slide */
    if ( ss != NULL && ss->single_monitor && !nw->show_no_slides ) {
        cursor = gtk_text_buffer_get_insert(nw->n->textbuf);
        gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &iter, cursor);
        gtk_text_iter_forward_to_tag_toggle(&iter, slide_tag(nw->n->textbuf));
        gtk_text_buffer_place_cursor(nw->n->textbuf, &iter);
    }

    set_clock_pos(nw);

    cursor = gtk_text_buffer_get_insert(nw->n->textbuf);
    gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &iter, cursor);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(nw->nv), &iter, 0, TRUE, 0, 0.5);

    /* Is the cursor on a slide? */
    anc = gtk_text_iter_get_child_anchor(&iter);
    if ( nw->show != NULL && anc != NULL ) {
        guint n;
        GtkWidget **th = gtk_text_child_anchor_get_widgets(anc, &n);
        assert(n == 1);
        sc_slideshow_set_slide(nw->show, gtk_thumbnail_get_slide(GTK_THUMBNAIL(th[0])));
        g_free(th);
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
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_BUFFER_ENDS, 1, FALSE);
    set_clock_pos(nw);
    update_toolbar(nw);
}


static void bold_sig(GSimpleAction *action, GVariant *parameter,
                     gpointer vp)
{
    //NarrativeWindow *nw = vp;
    //set_text_type(nw, TEXT_RUN_BOLD);
}


static void italic_sig(GSimpleAction *action, GVariant *parameter,
                       gpointer vp)
{
    //NarrativeWindow *nw = vp;
    //set_text_type(nw, TEXT_RUN_ITALIC);
}


static void underline_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    //NarrativeWindow *nw = vp;
    //set_text_type(nw, TEXT_RUN_UNDERLINE);
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


static void print_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    run_printing(nw->n, GTK_WIDGET(nw));
    gtk_narrative_view_redraw(GTK_NARRATIVE_VIEW(nw->nv));
}


static void changed_sig(GtkTextBuffer *buf, NarrativeWindow *nw)
{
    nw->n->saved = 0;
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
        gtk_window_close(GTK_WINDOW(nw->slidewindows[i]));
    }
    return FALSE;
}


static gboolean nw_key_press_sig(GtkEventControllerKey *self,
                                 guint keyval,
                                 guint keycode,
                                 GdkModifierType state,
                                 NarrativeWindow *nw)
{
    switch ( keyval ) {

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
            gtk_window_close(GTK_WINDOW(nw->show));
            return TRUE;
        }
        break;

    }

    return FALSE;
}


static gboolean ss_destroy_sig(GtkWidget *da, NarrativeWindow *nw)
{
    nw->show = NULL;

    gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);

    return FALSE;
}


static void start_slideshow(NarrativeWindow *nw, int no_slides)
{
    GtkEventController *evc;

    nw->show = sc_slideshow_new(nw->n, GTK_APPLICATION(nw->app));
    if ( nw->show == NULL ) return;

    evc = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(nw->show), evc);
    g_signal_connect(G_OBJECT(evc), "key-pressed",
         G_CALLBACK(nw_key_press_sig), nw);

    if ( no_slides ) {
        gtk_widget_set_visible(GTK_WIDGET(nw->show), FALSE);
        nw->show_no_slides = 1;
    }

    g_signal_connect(G_OBJECT(nw->show), "destroy",
         G_CALLBACK(ss_destroy_sig), nw);

    update_toolbar(nw);
    gtk_widget_grab_focus(GTK_WIDGET(nw->nv));
}


static void start_slideshow_here_sig(GSimpleAction *action, GVariant *parameter,
                                     gpointer vp)
{
    NarrativeWindow *nw = vp;
    start_slideshow(nw, 0);
    /* FIXME: Look backwards to the last slide, and set it */
    // sc_slideshow_set_slide(nw->show, narrative_get_slide_by_number(nw->n, 0));
}


static void start_slideshow_noslides_sig(GSimpleAction *action, GVariant *parameter,
                                         gpointer vp)
{
    NarrativeWindow *nw = vp;
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
    start_slideshow(nw, 1);
}


static void start_slideshow_sig(GSimpleAction *action, GVariant *parameter,
                                gpointer vp)
{
    NarrativeWindow *nw = vp;
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
    start_slideshow(nw, 0);
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
};


static void narrativewindow_class_init(NarrativeWindowClass *klass)
{
}


static void narrativewindow_init(NarrativeWindow *sw)
{
}


static void add_thumbnails(GtkTextView *tv, NarrativeWindow *nw)
{
    int i;

    for ( i=0; i<nw->n->n_slides; i++ ) {
        GtkWidget *th = gtk_thumbnail_new(nw->n->slides[i], nw);
        gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(tv),
                                          GTK_WIDGET(th),
                                          nw->n->slides[i]->anchor);
    }
}


NarrativeWindow *narrative_window_new(Narrative *n, GFile *file, GApplication *app)
{
    NarrativeWindow *nw;
    GtkWidget *vbox;
    GtkWidget *scroll;
    GtkWidget *toolbar;
    GtkWidget *button;
    GtkEventController *evc;

    nw = g_object_new(GTK_TYPE_NARRATIVE_WINDOW, "application", app, NULL);

    nw->app = app;
    nw->n = n;
    nw->n_slidewindows = 0;
    nw->file = file;
    if ( file != NULL ) g_object_ref(file);

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(nw), TRUE);

    update_titlebar(nw);

    g_action_map_add_action_entries(G_ACTION_MAP(nw), nw_entries,
                                    G_N_ELEMENTS(nw_entries), nw);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(nw), vbox);

    nw->nv = gtk_text_view_new();
    gtk_widget_set_vexpand(GTK_WIDGET(nw->nv), TRUE);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(nw->nv), n->textbuf);
    add_thumbnails(GTK_TEXT_VIEW(nw->nv), nw);

    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(GTK_WIDGET(toolbar), "toolbar");
    gtk_box_prepend(GTK_BOX(vbox), GTK_WIDGET(toolbar));

    /* Fullscreen */
    button = gtk_button_new_from_icon_name("view-fullscreen");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.startslideshow");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));

    button = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));

    /* Add slide */
    button = gtk_button_new_from_icon_name("list-add");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.slide");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));

    button = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));

    nw->bfirst = gtk_button_new_from_icon_name("go-top");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(nw->bfirst));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bfirst),
                                   "win.first");

    nw->bprev = gtk_button_new_from_icon_name("go-up");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(nw->bprev));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bprev),
                                   "win.prev");

    nw->bnext = gtk_button_new_from_icon_name("go-down");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(nw->bnext));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->bnext),
                                   "win.next");

    nw->blast = gtk_button_new_from_icon_name("go-bottom");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(nw->blast));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(nw->blast),
                                   "win.last");

    button = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));

    button = gtk_button_new_from_icon_name("format-text-bold");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.bold");

    button = gtk_button_new_from_icon_name("format-text-italic");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.italic");

    button = gtk_button_new_from_icon_name("format-text-underline");
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.underline");

    gtk_widget_set_sensitive(GTK_WIDGET(nw->bfirst), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(nw->bprev), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(nw->bnext), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(nw->blast), FALSE);

    evc = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(nw->nv), evc);

    scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(nw->nv));

    g_signal_connect(G_OBJECT(n->textbuf), "changed",
                     G_CALLBACK(changed_sig), nw);
    g_signal_connect(G_OBJECT(evc), "key-pressed",
             G_CALLBACK(nw_key_press_sig), nw);
    g_signal_connect(G_OBJECT(nw), "destroy",
             G_CALLBACK(nw_destroy_sig), nw);

    gtk_window_set_default_size(GTK_WINDOW(nw), 768, 768);
    gtk_box_append(GTK_BOX(vbox), scroll);
    gtk_widget_set_focus_child(GTK_WIDGET(scroll), GTK_WIDGET(nw->nv));
    gtk_widget_set_focus_child(GTK_WIDGET(vbox), GTK_WIDGET(scroll));

    return nw;
}
