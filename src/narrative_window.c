/*
 * narrative_window.c
 *
 * Copyright © 2014-2025 Thomas White <taw@bitwiz.me.uk>
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
#include "testcard.h"
#include "pr_clock.h"
#include "print.h"
#include "thumbnailwidget.h"

G_DEFINE_FINAL_TYPE(NarrativeWindow, colloquium_narrative_window, GTK_TYPE_APPLICATION_WINDOW)

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
    if ( gtk_text_buffer_get_modified(nw->n->textbuf) ) strcat(title, " *");
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
        if ( nw->slidewindows[i] == COLLOQUIUM_SLIDE_WINDOW(sw) ) {
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


static void saveas_response_sig(GObject *d, GAsyncResult *res, gpointer vp)
{
    NarrativeWindow *nw = vp;
    GFile *file;

    file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(d), res, NULL);
    if ( file == NULL ) return;

    if ( narrative_save(nw->n, file) ) {
        show_error(nw, _("Failed to save presentation"));
    }

    if ( nw->file != file ) {
        if ( nw->file != NULL ) g_object_unref(nw->file);
        nw->file = file;
        g_object_ref(nw->file);
    }
    g_object_unref(file);

    update_titlebar(nw);
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
    }
}


static void add_slide_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    NarrativeWindow *nw = vp;
    if ( nw->slide_sorter == NULL ) {
        nw->slide_sorter = slide_sorter_new(nw);
    }
    gtk_window_present(GTK_WINDOW(nw->slide_sorter));
}

static void add_heading(NarrativeWindow *nw, const char *name, const char *placeholder)
{
    GtkTextMark *cursor;
    GtkTextIter iter, end;

    cursor = gtk_text_buffer_get_insert(nw->n->textbuf);
    gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &iter, cursor);
    gtk_text_iter_forward_line(&iter);
    gtk_text_buffer_insert(nw->n->textbuf, &iter, placeholder, -1);

    gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &iter, cursor);
    gtk_text_iter_forward_line(&iter);
    end = iter;
    gtk_text_iter_forward_chars(&end, 16);

    gtk_text_buffer_apply_tag_by_name(nw->n->textbuf, name, &iter, &end);

    gtk_widget_grab_focus(GTK_WIDGET(nw->nv));
}


static void add_prestitle_sig(GSimpleAction *action, GVariant *parameter,
                              gpointer vp)
{
    NarrativeWindow *nw = vp;
    add_heading(nw, "prestitle", "Presentation title");
}


static void add_segstart_sig(GSimpleAction *action, GVariant *parameter,
                             gpointer vp)
{
    NarrativeWindow *nw = vp;
    add_heading(nw, "segstart", "Start of section");
}


static void add_segend_sig(GSimpleAction *action, GVariant *parameter,
                           gpointer vp)
{
}


static void add_eop_sig(GSimpleAction *action, GVariant *parameter,
                        gpointer vp)
{
}


static void set_clock_pos(NarrativeWindow *nw)
{
    int pos = get_cursor_para(GTK_TEXT_VIEW(nw->nv));
    int end = num_items_to_eop(nw->n);
    if ( pos >= end ) pos = end-1;
    pr_clock_set_pos(nw->pr_clock, pos, end);
}


static void update_highlight(NarrativeWindow *nw)
{
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(nw->n->textbuf, &start, &end);
    gtk_text_buffer_remove_tag_by_name(nw->n->textbuf, "highlight", &start, &end);

    if ( !nw->presenting ) return;

    GtkTextMark *cursor;
    cursor = gtk_text_buffer_get_insert(nw->n->textbuf);
    gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &start, cursor);

    end = start;
    gtk_text_iter_forward_to_line_end(&end);

    gtk_text_buffer_apply_tag_by_name(nw->n->textbuf, "highlight", &start, &end);
}


static void set_presenting_slide(NarrativeWindow *nw, Slide *s)
{
    int i;
    nw->presenting_slide = s;
    for ( i=0; i<nw->n_slidewindows; i++ ) {
        slide_window_set_slide(nw->slidewindows[i], s);
    }
}


static void reverse_paragraph(NarrativeWindow *nw)
{
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPHS, -1, FALSE);
    set_clock_pos(nw);
    update_highlight(nw);
}


static void advance_paragraph(NarrativeWindow *nw)
{
    GtkTextMark *cursor;
    GtkTextIter iter;
    GtkTextChildAnchor *anc;

    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPHS, 1, FALSE);
    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPH_ENDS, -1, FALSE);

    set_clock_pos(nw);

    cursor = gtk_text_buffer_get_insert(nw->n->textbuf);
    gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &iter, cursor);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(nw->nv), &iter, 0, TRUE, 0, 0.5);

    update_highlight(nw);

    /* Is the cursor on a slide? */
    anc = gtk_text_iter_get_child_anchor(&iter);
    if ( anc != NULL ) {
        guint n;
        GtkWidget **th = gtk_text_child_anchor_get_widgets(anc, &n);
        assert(n == 1);
        set_presenting_slide(nw, thumbnail_get_slide(COLLOQUIUM_THUMBNAIL(th[0])));
        g_free(th);
    }
}


static void toggle_tag(GtkTextBuffer *buf, const char *name)
{
    GtkTextIter start, end;
    GtkTextTag *tag = lookup_tag(buf, name);

    gtk_text_buffer_get_selection_bounds(buf, &start, &end);
    if ( gtk_text_iter_has_tag(&start, tag)
      || gtk_text_iter_has_tag(&end, tag)
      || gtk_text_iter_starts_tag(&start, tag)
      || gtk_text_iter_ends_tag(&end, tag) )
    {
        gtk_text_buffer_remove_tag(buf, tag, &start, &end);
    } else {
        gtk_text_buffer_apply_tag(buf, tag, &start, &end);
    }
    gtk_text_buffer_set_modified(buf, TRUE);
}


static void bold_sig(GSimpleAction *action, GVariant *parameter,
                     gpointer vp)
{
    NarrativeWindow *nw = vp;
    toggle_tag(nw->n->textbuf, "bold");
}


static void italic_sig(GSimpleAction *action, GVariant *parameter,
                       gpointer vp)
{
    NarrativeWindow *nw = vp;
    toggle_tag(nw->n->textbuf, "italic");
}


static void underline_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    NarrativeWindow *nw = vp;
    toggle_tag(nw->n->textbuf, "underline");
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


static void undo_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "text.undo", NULL);
}


static void redo_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "text.redo", NULL);
}


static void cut_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "clipboard.cut", NULL);
}


static void copy_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "clipboard.copy", NULL);
}


static void paste_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "clipboard.paste", NULL);
}


static void delete_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "selection.delete", NULL);
}


static void selectall_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    gtk_widget_activate_action(nw->nv, "selection.select-all", NULL);
}


static void print_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    NarrativeWindow *nw = vp;
    run_printing(nw->n, GTK_WIDGET(nw));
}


static void modified_changed_sig(GtkTextBuffer *buf, NarrativeWindow *nw)
{
    update_titlebar(nw);
}


static void changed_sig(GtkTextBuffer *buf, NarrativeWindow *nw)
{
    narrative_update_timing(GTK_TEXT_VIEW(nw->nv), nw->n);
    gtk_widget_queue_draw(GTK_WIDGET(nw->timing_ruler));
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


static void confirm_chosen(GObject *source, GAsyncResult *res, gpointer vp)
{
    NarrativeWindow *nw = vp;
    GError *error = NULL;
    int i;
    i = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), res, &error);
    if ( i == 0 ) {
        /* Not really, but user doesn't want to save */
        gtk_text_buffer_set_modified(nw->n->textbuf, FALSE);
        gtk_window_close(GTK_WINDOW(nw));
    } else if ( i == 1 ) {
        if ( narrative_save(nw->n, nw->file) ) {
            show_error(nw, _("Failed to save presentation"));
        } else {
            gtk_window_close(GTK_WINDOW(nw));
        }
    } /* else i == 2, do nothing */
}


static gboolean nw_close_request_sig(GtkWidget *self, NarrativeWindow *nw)
{
    if ( gtk_text_buffer_get_modified(nw->n->textbuf) ) {
        GtkAlertDialog *c;
        const char *const buttons[] = {_("Discard"), ("Save"), _("Cancel"), NULL};
        c = gtk_alert_dialog_new(_("Document not saved, really exit?"));
        gtk_alert_dialog_set_buttons(c, buttons);
        gtk_alert_dialog_set_default_button(c, 2);
        gtk_alert_dialog_choose(c, GTK_WINDOW(self), NULL, confirm_chosen, nw);
        return TRUE;
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
        if ( nw->presenting ) {
            scroll_down(nw);
            return TRUE;
        }
        break;

        case GDK_KEY_Page_Up :
        case GDK_KEY_Left :
        if ( nw->presenting ) {
            reverse_paragraph(nw);
            return TRUE;
        }
        break;

        case GDK_KEY_Page_Down :
        case GDK_KEY_Right :
        if ( nw->presenting ) {
            advance_paragraph(nw);
            return TRUE;
        }
        break;

    }

    return FALSE;
}


static void nw_laser_on_sig(SlideWindow *sw, gdouble x, gdouble y, NarrativeWindow *nw)
{
    int i;
    for ( i=0; i<nw->n_slidewindows; i++ ) {
        slide_window_set_laser(nw->slidewindows[i], x, y);
    }
}


static void nw_laser_off_sig(SlideWindow *sw, NarrativeWindow *nw)
{
    int i;
    for ( i=0; i<nw->n_slidewindows; i++ ) {
        slide_window_set_laser_off(nw->slidewindows[i]);
    }
}


static void nw_laser_move_sig(SlideWindow *sw, gdouble x, gdouble y, NarrativeWindow *nw)
{
    int i;
    for ( i=0; i<nw->n_slidewindows; i++ ) {
        slide_window_set_laser(nw->slidewindows[i], x, y);
    }
}


static SlideWindow *open_slide_window(NarrativeWindow *nw, Slide *slide)
{
    if ( nw->n_slidewindows < 16 ) {
        SlideWindow *sw = slide_window_new(nw->n, slide, nw, nw->app);
        nw->slidewindows[nw->n_slidewindows++] = sw;
        g_signal_connect(G_OBJECT(sw), "destroy",
                         G_CALLBACK(slide_window_closed_sig), nw);
        GtkEventController *evk = gtk_event_controller_key_new();
        gtk_widget_add_controller(GTK_WIDGET(sw), evk);
        g_signal_connect(G_OBJECT(evk), "key-pressed", G_CALLBACK(nw_key_press_sig), nw);
        g_signal_connect(G_OBJECT(sw), "laser-on", G_CALLBACK(nw_laser_on_sig), nw);
        g_signal_connect(G_OBJECT(sw), "laser-off", G_CALLBACK(nw_laser_off_sig), nw);
        g_signal_connect(G_OBJECT(sw), "laser-moved", G_CALLBACK(nw_laser_move_sig), nw);
        gtk_window_present(GTK_WINDOW(sw));
        return sw;
    } else {
        fprintf(stderr, _("Too many slide windows\n"));
        return NULL;
    }
}


static void openslide_sig(GSimpleAction *action, GVariant *parameter,
                          gpointer vp)
{
    NarrativeWindow *nw = vp;
    if ( nw->presenting_slide != NULL ) {
        open_slide_window(nw, nw->presenting_slide);
    } else {
        Slide *f = narrative_get_first_slide(nw->n);
        if ( f != NULL ) {
            open_slide_window(nw, f);
        } else {
            show_error(nw, "No slides!");
        }
    }
}


static gboolean nw_destroy_sig(GtkWidget *da, NarrativeWindow *nw)
{
    int i;
    if ( nw->pr_clock != NULL ) pr_clock_destroy(nw->pr_clock);
    for ( i=0; i<nw->n_slidewindows; i++ ) {
        gtk_window_close(GTK_WINDOW(nw->slidewindows[i]));
    }
    g_object_unref(nw->settings);
    return FALSE;
}


static void stop_presenting(NarrativeWindow *nw)
{
    if ( !nw->presenting ) return;
    nw->presenting = 0;
    update_highlight(nw);
    gtk_widget_unparent(nw->presenting_label);
    nw->presenting_slide = NULL;

    /* Put focus back in editor (not the toolbar) */
    gtk_widget_grab_focus(GTK_WIDGET(nw->nv));
}


static gboolean presenting_click_sig(GtkWidget *da, NarrativeWindow *nw)
{
    stop_presenting(nw);
    return FALSE;
}


static void start_presenting(NarrativeWindow *nw)
{
    GtkWidget *label;

    if ( nw->presenting ) return;
    nw->presenting = 1;

    label = gtk_label_new("Presenting (click to stop)");
    gtk_label_set_markup(GTK_LABEL(label),
                "<span background='#33ff33'><b>Presenting (click to stop)</b></span>");
    nw->presenting_label = gtk_button_new();
    gtk_button_set_child(GTK_BUTTON(nw->presenting_label), label);
    gtk_box_append(GTK_BOX(nw->toolbar), nw->presenting_label);
    g_signal_connect(G_OBJECT(nw->presenting_label), "clicked", G_CALLBACK(presenting_click_sig), nw);

    update_highlight(nw);
    gtk_widget_grab_focus(GTK_WIDGET(nw->nv));
}


static void start_presenting_here_sig(GSimpleAction *action, GVariant *parameter,
                                      gpointer vp)
{
    NarrativeWindow *nw = vp;
    GtkTextMark *cursor;
    GtkTextIter iter;

    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_PARAGRAPH_ENDS, -1, FALSE);
    start_presenting(nw);

    /* Look backwards to the last slide, and set it */
    cursor = gtk_text_buffer_get_insert(nw->n->textbuf);
    gtk_text_buffer_get_iter_at_mark(nw->n->textbuf, &iter, cursor);
    if ( gtk_text_iter_backward_to_tag_toggle(&iter, lookup_tag(nw->n->textbuf, "slide")) ) {

        guint n;
        GtkWidget **th;
        GtkTextChildAnchor *anc;

        gtk_text_iter_backward_cursor_position(&iter);
        anc = gtk_text_iter_get_child_anchor(&iter);
        if ( anc == NULL ) {
            fprintf(stderr, "No anchor found despite slide tag!\n");
            return;
        }
        th = gtk_text_child_anchor_get_widgets(anc, &n);
        assert(n == 1);
        if ( nw->n_slidewindows == 0 ) {
            open_slide_window(nw, thumbnail_get_slide(COLLOQUIUM_THUMBNAIL(th[0])));
        } else {
            set_presenting_slide(nw, thumbnail_get_slide(COLLOQUIUM_THUMBNAIL(th[0])));
        }

        g_free(th);
    }
}


static void start_presenting_sig(GSimpleAction *action, GVariant *parameter,
                                 gpointer vp)
{
    NarrativeWindow *nw = vp;
    Slide *slide;

    g_signal_emit_by_name(G_OBJECT(nw->nv), "move-cursor",
            GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
    start_presenting(nw);

    slide = narrative_get_first_slide(nw->n);
    if ( slide != NULL ) {
        if ( nw->n_slidewindows == 0 ) {
            open_slide_window(nw, slide);
        } else {
            set_presenting_slide(nw, slide);
        }
    } /* Else we are in presentation mode, but no slides */
}


static void fsslide_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    int n;
    NarrativeWindow *nw = vp;
    GListModel *monitors;
    GdkMonitor *mon;
    SlideWindow *sw;

    g_variant_get(parameter, "u", &n);

    monitors = gdk_display_get_monitors(gdk_display_get_default());
    mon = GDK_MONITOR(g_list_model_get_object(monitors, n));
    if ( mon == NULL ) {
        show_error(nw, "Could not find monitor");
        return;
    }

    if ( nw->presenting_slide != NULL ) {
        sw = open_slide_window(nw, nw->presenting_slide);
    } else {
        Slide *f = narrative_get_first_slide(nw->n);
        if ( f != NULL ) {
            sw = open_slide_window(nw, f);
        } else {
            show_error(nw, "No slides!");
            sw = NULL;
        }
    }

    if ( sw != NULL ) {
        slide_window_fullscreen_on_monitor(sw, mon);
    }
}


static void draw_timing_ruler(GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer vp)
{
    NarrativeWindow *nw = vp;
    int i;
    double scroll_pos;
    PangoLayout *layout;
    PangoFontDescription *fontdesc;
    GdkRGBA col;

    /* Foreground */
    gtk_widget_get_color(GTK_WIDGET(da), &col);

    /* Background */
    if ( col.red+col.blue+col.green < 1.5 ) {
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
    } else {
        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 1.0);
    }
    cairo_rectangle(cr, 0.0, 0.0, w, h);
    cairo_fill(cr);

    scroll_pos = gtk_adjustment_get_value(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(nw->nv)));

    layout = pango_layout_new(gtk_widget_get_pango_context(GTK_WIDGET(nw->nv)));
    fontdesc = pango_font_description_from_string("Sans 12");
    pango_layout_set_font_description(layout, fontdesc);

    for ( i=0; i<nw->n->n_time_marks; i++ ) {

        char tmp[64];
        double y;

        y = nw->n->time_marks[i].y-scroll_pos;

        cairo_move_to(cr, 0.0, y);
        cairo_line_to(cr, 20.0, y);
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, col.red, col.green, col.blue);
        cairo_stroke(cr);

        snprintf(tmp, 63, _("%.0f min"), nw->n->time_marks[i].minutes);
        cairo_move_to(cr, 5.0, y+2.0);
        pango_layout_set_text(layout, tmp, -1);
        cairo_set_source_rgb(cr, col.red, col.green, col.blue);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        cairo_fill(cr);

    }

    g_object_unref(layout);
    pango_font_description_free(fontdesc);
}


GActionEntry nw_entries[] = {

    { "about", nw_about_sig, NULL, NULL, NULL },
    { "save", save_sig, NULL, NULL, NULL },
    { "saveas", saveas_sig, NULL, NULL, NULL },
    { "slide", add_slide_sig, NULL, NULL, NULL },
    { "eop", add_eop_sig, NULL, NULL, NULL },
    { "prestitle", add_prestitle_sig, NULL, NULL, NULL },
    { "segstart", add_segstart_sig, NULL, NULL, NULL },
    { "segend", add_segend_sig, NULL, NULL, NULL },
    { "startslideshow", start_presenting_sig, NULL, NULL, NULL },
    { "startslideshowhere", start_presenting_here_sig, NULL, NULL, NULL },
    { "clock", open_clock_sig, NULL, NULL, NULL },
    { "testcard", testcard_sig, NULL, NULL, NULL },
    { "openslide", openslide_sig, NULL, NULL, NULL },
    { "bold", bold_sig, NULL, NULL, NULL },
    { "italic", italic_sig, NULL, NULL, NULL },
    { "underline", underline_sig, NULL, NULL, NULL },
    { "print", print_sig, NULL, NULL, NULL  },
    { "undo", undo_sig, NULL, NULL, NULL  },
    { "redo", redo_sig, NULL, NULL, NULL  },
    { "cut", cut_sig, NULL, NULL, NULL  },
    { "copy", copy_sig, NULL, NULL, NULL  },
    { "paste", paste_sig, NULL, NULL, NULL  },
    { "delete", delete_sig, NULL, NULL, NULL  },
    { "select-all", selectall_sig, NULL, NULL, NULL  },
    { "fsslidem", NULL, NULL, NULL, NULL },
    { "fsslide", fsslide_sig, "u", NULL, NULL },
};


static void colloquium_narrative_window_class_init(NarrativeWindowClass *klass)
{
}


static void colloquium_narrative_window_init(NarrativeWindow *sw)
{
}


static void thumbnail_click_sig(GtkGestureClick *self, int n_press,
                                gdouble x, gdouble y, gpointer vp)
{
    Thumbnail *th = vp;

    if ( n_press != 2 ) return;

    if ( !th->nw->presenting ) {
        open_slide_window(th->nw, th->slide);
    } else {
        set_presenting_slide(th->nw, th->slide);
    }
}


static void add_thumbnails(GtkTextView *tv, NarrativeWindow *nw)
{
    int i;

    for ( i=0; i<nw->n->n_slides; i++ ) {
        GtkWidget *th = thumbnail_new(nw->n->slides[i], nw);
        GtkGesture *evc = gtk_gesture_click_new();
        gtk_widget_add_controller(GTK_WIDGET(th), GTK_EVENT_CONTROLLER(evc));
        g_signal_connect(G_OBJECT(evc), "pressed", G_CALLBACK(thumbnail_click_sig), th);

        float asp = slide_get_aspect(nw->n->slides[i]);
        if ( 320*asp > 512 ) {
            thumbnail_set_slide_width(COLLOQUIUM_THUMBNAIL(th), 512);
        } else {
            thumbnail_set_slide_height(COLLOQUIUM_THUMBNAIL(th), 320);
        }
        gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(tv),
                                          GTK_WIDGET(th),
                                          nw->n->slides[i]->anchor);
    }
}


static void scroll_update(GtkAdjustment *adj, GtkDrawingArea *da)
{
    gtk_widget_queue_draw(GTK_WIDGET(da));
}



static gboolean drop_thumnail(NarrativeWindow *nw, double x, double y, Thumbnail *th)
{
    int bx, by;
    GtkTextIter iter;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(nw->nv), GTK_TEXT_WINDOW_TEXT, x, y, &bx, &by);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(nw->nv), &iter, bx, by);
    gtk_text_iter_forward_line(&iter);

    Slide *slide = slide_copy(th->slide);
    insert_slide_anchor(nw->n->textbuf, slide, iter, 1);
    GtkWidget *thn = thumbnail_new(slide, nw);
    gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(nw->nv), GTK_WIDGET(thn), slide->anchor);
    GtkGesture *evc = gtk_gesture_click_new();
    gtk_widget_add_controller(GTK_WIDGET(thn), GTK_EVENT_CONTROLLER(evc));
    g_signal_connect(G_OBJECT(evc), "pressed", G_CALLBACK(thumbnail_click_sig), thn);

    return TRUE;
}


static gboolean drop_file(NarrativeWindow *nw, double x, double y, GFile *file)
{
    int bx, by;
    GtkTextIter iter;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(nw->nv), GTK_TEXT_WINDOW_TEXT, x, y, &bx, &by);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(nw->nv), &iter, bx, by);
    gtk_text_iter_forward_line(&iter);

    Slide *slide = slide_new();
    slide->ext_file = g_file_dup(file);
    insert_slide_anchor(nw->n->textbuf, slide, iter, 1);
    GtkWidget *thn = thumbnail_new(slide, nw);
    gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(nw->nv), GTK_WIDGET(thn), slide->anchor);
    GtkGesture *evc = gtk_gesture_click_new();
    gtk_widget_add_controller(GTK_WIDGET(thn), GTK_EVENT_CONTROLLER(evc));
    g_signal_connect(G_OBJECT(evc), "pressed", G_CALLBACK(thumbnail_click_sig), thn);

    return TRUE;
}


static gboolean drop_sig(GtkDropTarget *drop, const GValue *val, double x, double y, gpointer vp)
{
    NarrativeWindow *nw = vp;

    if ( G_VALUE_HOLDS(val, COLLOQUIUM_TYPE_THUMBNAIL) ) {
        return drop_thumnail(nw, x, y, COLLOQUIUM_THUMBNAIL(g_value_get_object(val)));
    }

    if ( G_VALUE_HOLDS(val, G_TYPE_FILE) ) {
        return drop_file(nw, x, y, G_FILE(g_value_get_object(val)));
    }

    return FALSE;
}


static void finish_nw(gpointer vp)
{
    NarrativeWindow *nw = vp;
    narrative_update_timing(GTK_TEXT_VIEW(nw->nv), nw->n);
    gtk_widget_queue_draw(GTK_WIDGET(nw->timing_ruler));
}


static void update_fsmenu(NarrativeWindow *nw)
{
    GMenu *m;
    GListModel *monitors;
    GdkMonitor *mon;
    int i = 0;

    monitors = gdk_display_get_monitors(gdk_display_get_default());
    if ( monitors == NULL ) {
        fprintf(stderr, "Failed to enumerate monitors\n");
        return;
    }

    m = gtk_application_get_menu_by_id(GTK_APPLICATION(nw->app), "fsslide");

    g_menu_remove_all(m);

    while ( (mon = g_list_model_get_item(monitors, i)) != NULL ) {

        GMenuItem *item = g_menu_item_new(gdk_monitor_get_description(mon), NULL);
        g_menu_item_set_action_and_target(item, "win.fsslide", "u", i, NULL);
        g_menu_append_item(m, item);

        i++;

    }
}


static void monitors_changed_sig(GListModel *monitors, guint pos, guint rem, guint add,
                                 NarrativeWindow *nw)
{
    update_fsmenu(nw);
}


NarrativeWindow *narrative_window_new(Narrative *n, GFile *file, GApplication *app)
{
    NarrativeWindow *nw;
    GtkWidget *vbox;
    GtkWidget *scroll;
    GtkWidget *button;
    GtkEventController *evc;
    GtkDropTarget *drop;
    GListModel *monitors;

    nw = g_object_new(COLLOQUIUM_TYPE_NARRATIVE_WINDOW, "application", app, NULL);

    nw->app = app;
    nw->n = n;
    nw->n_slidewindows = 0;
    nw->file = file;
    nw->slide_sorter = NULL;
    nw->presenting = 0;
    nw->presenting_slide = NULL;
    if ( file != NULL ) g_object_ref(file);

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(nw), TRUE);

    g_action_map_add_action_entries(G_ACTION_MAP(nw), nw_entries,
                                    G_N_ELEMENTS(nw_entries), nw);

    nw->settings = g_settings_new("uk.me.bitwiz.colloquium");

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(nw), vbox);

    nw->nv = gtk_text_view_new();
    gtk_widget_set_vexpand(GTK_WIDGET(nw->nv), TRUE);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(nw->nv), n->textbuf);
    add_thumbnails(GTK_TEXT_VIEW(nw->nv), nw);
    gtk_text_buffer_set_modified(n->textbuf, FALSE);

    gtk_widget_add_css_class(nw->nv, "narrative");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(nw->nv), GTK_WRAP_WORD);
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(nw->nv), 10);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(nw->nv), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(nw->nv), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(nw->nv), 10);

    nw->timing_ruler = gtk_drawing_area_new();
    gtk_text_view_set_gutter(GTK_TEXT_VIEW(nw->nv), GTK_TEXT_WINDOW_LEFT, nw->timing_ruler);
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(nw->timing_ruler), 100);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(nw->timing_ruler), draw_timing_ruler, nw, NULL);

    nw->toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(GTK_WIDGET(nw->toolbar), "toolbar");
    gtk_box_prepend(GTK_BOX(vbox), GTK_WIDGET(nw->toolbar));

    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.save",
            (const char *[])  {"<Control>s", NULL});

    /* Fullscreen */
    button = gtk_button_new_from_icon_name("view-fullscreen");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.startslideshow");
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));

    button = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));

    /* Add slide */
    button = gtk_button_new_from_icon_name("list-add");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.slide");
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));

    button = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));

    button = gtk_button_new_from_icon_name("format-text-bold");
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.bold");

    button = gtk_button_new_from_icon_name("format-text-italic");
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.italic");

    button = gtk_button_new_from_icon_name("format-text-underline");
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button),
                                   "win.underline");

    button = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(nw->toolbar), GTK_WIDGET(button));

    evc = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(nw->nv), evc);

    drop = gtk_drop_target_new(COLLOQUIUM_TYPE_THUMBNAIL, GDK_ACTION_COPY);
    GType types[2];
    types[0] = COLLOQUIUM_TYPE_THUMBNAIL;
    types[1] = G_TYPE_FILE;
    gtk_drop_target_set_gtypes(drop, types, 2);
    gtk_widget_add_controller(GTK_WIDGET(nw->nv), GTK_EVENT_CONTROLLER(drop));
    g_signal_connect(G_OBJECT(drop), "drop", G_CALLBACK(drop_sig), nw);

    scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(nw->nv));

    GtkAdjustment *adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(nw->nv));
    g_signal_connect(G_OBJECT(adj), "value-changed", G_CALLBACK(scroll_update), nw->timing_ruler);

    g_signal_connect_after(G_OBJECT(n->textbuf), "changed", G_CALLBACK(changed_sig), nw);
    g_signal_connect_after(G_OBJECT(n->textbuf), "modified-changed",
                                    G_CALLBACK(modified_changed_sig), nw);
    g_signal_connect(G_OBJECT(evc), "key-pressed", G_CALLBACK(nw_key_press_sig), nw);
    g_signal_connect(G_OBJECT(nw), "destroy", G_CALLBACK(nw_destroy_sig), nw);
    g_signal_connect(G_OBJECT(nw), "close-request", G_CALLBACK(nw_close_request_sig), nw);

    gtk_window_set_default_size(GTK_WINDOW(nw), 512, 768);
    gtk_box_append(GTK_BOX(vbox), scroll);
    gtk_widget_set_focus_child(GTK_WIDGET(scroll), GTK_WIDGET(nw->nv));
    gtk_widget_set_focus_child(GTK_WIDGET(vbox), GTK_WIDGET(scroll));

    update_titlebar(nw);
    update_fsmenu(nw);
    g_idle_add_once(finish_nw, nw);

    monitors = gdk_display_get_monitors(gdk_display_get_default());
    g_signal_connect(G_OBJECT(monitors), "items-changed", G_CALLBACK(monitors_changed_sig), nw);

    return nw;
}
