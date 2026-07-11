/*
 * prefswindow.c
 *
 * Copyright © 2025 Thomas White <taw@bitwiz.org.uk>
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

#include <libintl.h>
#define _(x) gettext(x)

#include "prefswindow.h"

G_DEFINE_FINAL_TYPE(PrefsWindow, colloquium_prefs_window, GTK_TYPE_WINDOW)


static void colloquium_prefs_window_class_init(PrefsWindowClass *klass)
{
}


static void colloquium_prefs_window_init(PrefsWindow *pw)
{
}

static void line_spacing_sig(GtkAdjustment *self, GSettings *settings)
{
    double v = gtk_adjustment_get_value(self);
    g_settings_set_double(settings, "narrative-line-spacing", v);
}


static void font_sig(GObject *self, GParamSpec *spec, GSettings *settings)
{
    PangoFontDescription *fontdesc;
    fontdesc = gtk_font_dialog_button_get_font_desc(GTK_FONT_DIALOG_BUTTON(self));
    g_settings_set_string(settings, "narrative-font", pango_font_description_to_string(fontdesc));
}


static void syscol_sig(GObject *self, GSettings *settings)
{
    g_settings_set_boolean(settings, "narrative-use-theme",
            gtk_check_button_get_active(GTK_CHECK_BUTTON(self)));
}


static void fgcol_sig(GObject *self, GParamSpec *spec, GSettings *settings)
{
    char *col;
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(self));
    col = gdk_rgba_to_string(rgba);
    g_settings_set_string(settings, "narrative-fg", col);
    free(col);
}


static void bgcol_sig(GObject *self, GParamSpec *spec, GSettings *settings)
{
    char *col;
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(self));
    col = gdk_rgba_to_string(rgba);
    g_settings_set_string(settings, "narrative-bg", col);
    free(col);
}


static void inv_maybe_disable(GtkWidget *toggle, GtkWidget *victim)
{
    gtk_widget_set_sensitive(victim, !gtk_check_button_get_active(GTK_CHECK_BUTTON(toggle)));
}


static void maybe_disable(GtkWidget *widget, GtkWidget *toggle)
{
    g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(inv_maybe_disable), widget);
	inv_maybe_disable(toggle, widget);
}


static void gutter_sig(GtkEntry *self, GSettings *settings)
{
    const char *txt = gtk_editable_get_text(GTK_EDITABLE(self));
    g_settings_set_uint(settings, "gutter-width", atoi(txt));
}



static GtkWidget *narrative_prefs(GSettings *settings)
{
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *fb;
    GtkWidget *ls;
    PangoFontDescription *fontdesc;
    GtkWidget *cb;
    GdkRGBA rgba;
    GtkWidget *toggle;
    GtkWidget *entry;
    GtkAdjustment *adj;
    char tmp[64];

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_box_set_spacing(GTK_BOX(box), 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Font:")));
    fb = gtk_font_dialog_button_new(gtk_font_dialog_new());
    gtk_box_append(GTK_BOX(hbox), fb);
    fontdesc = pango_font_description_from_string(g_settings_get_string(settings, "narrative-font"));
    gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(fb), fontdesc);
    pango_font_description_free(fontdesc);
    g_signal_connect(G_OBJECT(fb), "notify::font-desc", G_CALLBACK(font_sig), settings);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Line spacing:")));
    ls = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, 2.0, 0.05);
    gtk_scale_set_digits(GTK_SCALE(ls), 2);
    gtk_scale_add_mark(GTK_SCALE(ls), 1.0, GTK_POS_BOTTOM, "Single");
    gtk_scale_add_mark(GTK_SCALE(ls), 1.5, GTK_POS_BOTTOM, "1.5");
    gtk_scale_add_mark(GTK_SCALE(ls), 2.0, GTK_POS_BOTTOM, "Double");
    gtk_widget_set_hexpand(ls, TRUE);
    gtk_widget_set_margin_start(ls, 15);
    gtk_widget_set_margin_end(ls, 15);
    gtk_box_append(GTK_BOX(hbox), ls);
    adj = gtk_range_get_adjustment(GTK_RANGE(ls));
    gtk_adjustment_set_value(adj, g_settings_get_double(settings, "narrative-line-spacing"));
    g_signal_connect(G_OBJECT(adj), "value-changed", G_CALLBACK(line_spacing_sig), settings);

    toggle = gtk_check_button_new_with_label(_("Use colours from system theme"));
    gtk_box_append(GTK_BOX(box), toggle);
    g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(syscol_sig), settings);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(toggle),
            g_settings_get_boolean(settings, "narrative-use-theme"));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Foreground:")));
    cb = gtk_color_dialog_button_new(gtk_color_dialog_new());
    gtk_box_append(GTK_BOX(hbox), cb);
    gdk_rgba_parse(&rgba, g_settings_get_string(settings, "narrative-fg"));
    gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(cb), &rgba);
    g_signal_connect(G_OBJECT(cb), "notify::rgba", G_CALLBACK(fgcol_sig), settings);
    maybe_disable(cb, toggle);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Background:")));
    cb = gtk_color_dialog_button_new(gtk_color_dialog_new());
    gtk_box_append(GTK_BOX(hbox), cb);
    gdk_rgba_parse(&rgba, g_settings_get_string(settings, "narrative-bg"));
    gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(cb), &rgba);
    g_signal_connect(G_OBJECT(cb), "notify::rgba", G_CALLBACK(bgcol_sig), settings);
    maybe_disable(cb, toggle);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Margin width:")));
    entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), entry);
    snprintf(tmp, 63, "%i", g_settings_get_uint(settings, "gutter-width"));
    gtk_editable_set_text(GTK_EDITABLE(entry), tmp);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(gutter_sig), settings);

    return box;
}


static void highlight_sig(GObject *self, GParamSpec *spec, GSettings *settings)
{
    char *col;
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(self));
    col = gdk_rgba_to_string(rgba);
    g_settings_set_string(settings, "highlight", col);
    free(col);
}


static void wpm_sig(GtkEntry *self, GSettings *settings)
{
    const char *txt = gtk_editable_get_text(GTK_EDITABLE(self));
    g_settings_set_double(settings, "words-per-minute", atof(txt));
}


static GtkWidget *presentation_prefs(GSettings *settings)
{
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *entry;
    char tmp[64];
    GtkWidget *cb;
    GdkRGBA rgba;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_box_set_spacing(GTK_BOX(box), 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Words per minute:")));
    entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(hbox), entry);
    snprintf(tmp, 63, "%.0f", g_settings_get_double(settings, "words-per-minute"));
    gtk_editable_set_text(GTK_EDITABLE(entry), tmp);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(wpm_sig), settings);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Text highlight colour:")));
    cb = gtk_color_dialog_button_new(gtk_color_dialog_new());
    gtk_box_append(GTK_BOX(hbox), cb);
    gdk_rgba_parse(&rgba, g_settings_get_string(settings, "highlight"));
    gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(cb), &rgba);
    g_signal_connect(G_OBJECT(cb), "notify::rgba", G_CALLBACK(highlight_sig), settings);

    return box;
}


static GFile *imagestore_as_gfile(GSettings *settings)
{
    char *s = g_settings_get_string(settings, "imagestore");
    if ( s == NULL ) return NULL;
    if ( s[0] == '\0' ) return NULL;
    return g_file_new_for_uri(s);
}


static char *g_file_display_name(GFile *file)
{
    GFileInfo *info;
    GError *error;
    const gchar *name;
    gchar *namecopy;

    if ( file == NULL ) return g_strdup("(none)");

    error = NULL;
    info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                             G_FILE_QUERY_INFO_NONE, NULL, &error);
    if ( info == NULL ) {
        if ( error->code == G_IO_ERROR_NOT_FOUND ) {
            return g_strdup("(not found)");
        } else {
            fprintf(stderr, _("Failed to read info: %s\n"), error->message);
            return g_strdup("(error)");
        }
    }
    name = g_file_info_get_attribute_string(info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
    namecopy = g_strdup(name);
    g_object_unref(info);
    return namecopy;
}


static void set_imagestore_button(GtkWidget *fc, GSettings *settings)
{
    GFile *file = imagestore_as_gfile(settings);
    gchar *name = g_file_display_name(file);
    gtk_button_set_label(GTK_BUTTON(fc), name);
    g_free(name);
    if ( file != NULL ) g_object_unref(file);
}


static void imagestore_finish(GObject *chooser, GAsyncResult *res, gpointer data)
{
    PrefsWindow *pw = data;
    GError *error = NULL;
    GFile *loc;

    loc = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(chooser), res, &error);
    if ( error == NULL ) {
        g_settings_set_string(pw->settings, "imagestore", g_file_get_uri(loc));
        set_imagestore_button(pw->imagestore_button, pw->settings);
    } else {
        if ( error->code != GTK_DIALOG_ERROR_DISMISSED ) {
            fprintf(stderr, "Error: %s\n", error->message);
        }
        g_error_free(error);
    }
}


static void imagestore_sig(GtkButton *button, PrefsWindow *pw)
{
    GtkFileDialog *d = gtk_file_dialog_new();
    GFile *file = imagestore_as_gfile(pw->settings);
    if ( file != NULL ) gtk_file_dialog_set_initial_file(d, file);
    gtk_file_dialog_select_folder(d, GTK_WINDOW(pw), NULL,
                                  imagestore_finish, pw);
    if ( file != NULL ) g_object_unref(file);
}


static void imagestore_clear_sig(GtkButton *button, PrefsWindow *pw)
{
    g_settings_set_string(pw->settings, "imagestore", "");
    set_imagestore_button(pw->imagestore_button, pw->settings);
}


static GtkWidget *location_prefs(PrefsWindow *pw)
{
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *button;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_box_set_spacing(GTK_BOX(box), 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_box_append(GTK_BOX(hbox), gtk_label_new(_("Location for slides/pictures:")));

    pw->imagestore_button = gtk_button_new_with_label("");
    set_imagestore_button(pw->imagestore_button, pw->settings);
    gtk_box_append(GTK_BOX(hbox), pw->imagestore_button);

    button = gtk_button_new_from_icon_name("edit-clear");
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(imagestore_clear_sig), pw);
    gtk_box_append(GTK_BOX(hbox), button);

    g_signal_connect(G_OBJECT(pw->imagestore_button), "clicked", G_CALLBACK(imagestore_sig), pw);

    return box;
}


static void close_sig(GtkWidget *widget, PrefsWindow *pw)
{
    gtk_window_close(GTK_WINDOW(pw));
}


PrefsWindow *prefs_window_new()
{
    PrefsWindow *pw;
    GtkWidget *notebook;
    GtkWidget *box;
    GtkWidget *hbox;
    GtkWidget *button;

    pw = g_object_new(COLLOQUIUM_TYPE_PREFS_WINDOW, NULL);
    pw->settings = g_settings_new("uk.me.bitwiz.colloquium");

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_window_set_child(GTK_WINDOW(pw), box);

    notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(box), notebook);
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             narrative_prefs(pw->settings),
                             gtk_label_new(_("Narrative")));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             presentation_prefs(pw->settings),
                             gtk_label_new(_("Presentation")));

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             location_prefs(pw),
                             gtk_label_new(_("Directories")));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_append(GTK_BOX(box), hbox);
    gtk_widget_set_halign(hbox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 8);
    gtk_widget_set_margin_bottom(hbox, 8);

    button = gtk_button_new_with_label(_("Close"));
    gtk_box_prepend(GTK_BOX(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_sig), pw);

    gtk_window_set_default_size(GTK_WINDOW(pw), 480, 320);
    gtk_window_set_title(GTK_WINDOW(pw), _("Preferences"));

    return pw;

}
