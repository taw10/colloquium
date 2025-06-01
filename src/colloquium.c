/*
 * colloquium.c
 *
 * Copyright © 2013-2025 Thomas White <taw@bitwiz.me.uk>
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
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libintl.h>
#define _(x) gettext(x)

#include "narrative.h"
#include "colloquium.h"
#include "narrative_window.h"
#include "prefswindow.h"


G_DEFINE_FINAL_TYPE(Colloquium, colloquium, GTK_TYPE_APPLICATION)


static void colloquium_activate(GApplication *papp);
static void colloquium_startup(GApplication *papp);
static void colloquium_open(GApplication *papp, GFile **files, gint n_files, const gchar *hint);

static void colloquium_class_init(ColloquiumClass *klass)
{
    GApplicationClass *app_klass = G_APPLICATION_CLASS(klass);
    app_klass->startup = colloquium_startup;
    app_klass->activate = colloquium_activate;
    app_klass->open = colloquium_open;
}


static void colloquium_init(Colloquium *app)
{
    app->hidepointer = 0;
    app->first_run = 0;
}


static void colloquium_activate(GApplication *papp)
{
    Colloquium *app = COLLOQUIUM(papp);
    if ( !app->first_run ) {
        NarrativeWindow *nw;
        Narrative *n = narrative_new();
        nw = narrative_window_new(n, NULL, papp);
        gtk_window_present(GTK_WINDOW(nw));
    }
}


static void new_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    GApplication *app = vp;
    g_application_activate(app);
}


static void open_intro_doc(Colloquium *app)
{
    GFile *file = g_file_new_for_uri("resource:///uk/me/bitwiz/colloquium/demo.md");
    g_application_open(G_APPLICATION(app), &file, 1, "");
    g_object_unref(file);
}


static void intro_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    GApplication *app = vp;
    open_intro_doc(COLLOQUIUM(app));
}


void open_about_dialog(GtkWidget *parent)
{
    GtkWidget *window;

    const gchar *authors[] = {
        "Thomas White <taw@bitwiz.me.uk>",
        NULL
    };

    window = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(parent));

    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(window),
        "Colloquium");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(window),
        "uk.me.bitwiz.colloquium");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window),
        PACKAGE_VERSION);
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(window),
        "© 2011-2025 Thomas White <taw@bitwiz.me.uk>");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(window),
        /* Description of the program */
        _("Narrative-based presentation system"));
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(window), GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(window),
        "https://www.bitwiz.me.uk/");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(window),
        "https://www.bitwiz.me.uk/");
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(window), authors);
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(window),
                                            _("translator-credits"));

    gtk_window_present(GTK_WINDOW(window));
}


static void closeit(gpointer d, gpointer vp)
{
    gtk_window_close(GTK_WINDOW(d));
}


static void quit_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    GApplication *app = vp;
    GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));
    g_list_foreach(windows, closeit, app);
}


static GFile **glistmodel_to_array(GListModel *list, int *n)
{
    int i = 0;
    int len = g_list_model_get_n_items(list);
    GFile **files = malloc(len * sizeof(GFile *));

    if ( files == NULL ) return NULL;

    for ( i=0; i<len; i++ ) {
        files[i] = g_list_model_get_item(list, i);
        g_object_ref(files[i]);
    }

    *n = len;
    return files;
}


static void open_response_sig(GObject *d, GAsyncResult  *res, gpointer vp)
{
    GListModel *files;
    GFile **files_array;
    int i, n_files;
    GApplication *app = vp;

    files = gtk_file_dialog_open_multiple_finish(GTK_FILE_DIALOG(d), res, NULL);
    if ( files == NULL ) return;

    files_array = glistmodel_to_array(files, &n_files);
    if ( files_array == NULL ) {
        fprintf(stderr, "Failed to convert file list\n");
        return;
    }
    g_object_unref(files);
    g_application_open(app, files_array, n_files, "");

    for ( i=0; i<n_files; i++ ) {
        g_object_unref(files_array[i]);
    }

    g_object_unref(d);
}


static void prefs_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    PrefsWindow *pw = prefs_window_new();
    gtk_window_present(GTK_WINDOW(pw));
}


static void open_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
    GtkFileDialog *d;
    GApplication *app = vp;

    d = gtk_file_dialog_new();

    gtk_file_dialog_set_title(d, _("Open Presentation"));
    gtk_file_dialog_set_accept_label(d, _("Open"));

    gtk_file_dialog_open_multiple(d,
                                  gtk_application_get_active_window(GTK_APPLICATION(app)),
                                  NULL, open_response_sig, app);
}


GActionEntry app_entries[] = {

    { "new", new_sig, NULL, NULL, NULL  },
    { "open", open_sig, NULL, NULL, NULL  },
    { "intro", intro_sig, NULL, NULL, NULL  },
    { "quit", quit_sig, NULL, NULL, NULL  },
    { "prefs", prefs_sig, NULL, NULL, NULL  },
};


static void colloquium_open(GApplication  *papp, GFile **files, gint n_files,
                            const gchar *hint)
{
    int i;

    for ( i=0; i<n_files; i++ ) {
        Narrative *n;
        n = narrative_load(files[i]);
        if ( n != NULL ) {
            NarrativeWindow *nw;
            nw = narrative_window_new(n, files[i], papp);
            gtk_window_present(GTK_WINDOW(nw));
        } else {
            char *uri = g_file_get_uri(files[i]);
            fprintf(stderr, _("Failed to load presentation '%s'\n"),
                    uri);
            g_free(uri);
        }
    }
}


static const char *pango_style_to_text(PangoFontDescription *fd)
{
    switch ( pango_font_description_get_style(fd) ) {
        case PANGO_STYLE_NORMAL : return "normal";
        case PANGO_STYLE_ITALIC : return "italic";
        case PANGO_STYLE_OBLIQUE : return "oblique";
        default : return "normal";
    }
}


static const char *pango_stretch_to_text(PangoFontDescription *fd)
{
    switch ( pango_font_description_get_stretch(fd) ) {
        case PANGO_STRETCH_ULTRA_CONDENSED : return "ultra-condensed";
        case PANGO_STRETCH_EXTRA_CONDENSED : return "extra-condensed";
        case PANGO_STRETCH_CONDENSED : return "condensed";
        case PANGO_STRETCH_SEMI_CONDENSED : return "semi-condensed";
        case PANGO_STRETCH_ULTRA_EXPANDED : return "ultra-expanded";
        case PANGO_STRETCH_EXTRA_EXPANDED : return "extra-expanded";
        case PANGO_STRETCH_EXPANDED : return "expanded";
        case PANGO_STRETCH_SEMI_EXPANDED : return "semi-expanded";
        default : return "normal";
    }
}



static void update_css(GSettings *settings, gchar *key, GtkCssProvider *provider)
{
    char css1[2048], css2[1024] = "";
    char *font;
    char *fgcol;
    char *bgcol;
    PangoFontDescription *fd;

    font = g_settings_get_string(settings, "narrative-font");
    fd = pango_font_description_from_string(font);
    fgcol = g_settings_get_string(settings, "narrative-fg");
    bgcol = g_settings_get_string(settings, "narrative-bg");
    snprintf(css1, 1023, ".narrative { font-family: %s; font-style: %s;"
                         " font-weight: %i;  font-stretch: %s; font-size: %ip%c; }",
            pango_font_description_get_family(fd),
            pango_style_to_text(fd),
            pango_font_description_get_weight(fd),
            pango_stretch_to_text(fd),
            pango_font_description_get_size(fd)/PANGO_SCALE,
            pango_font_description_get_size_is_absolute(fd)?'x':'t');
    pango_font_description_free(fd);

    if ( !g_settings_get_boolean(settings, "narrative-use-theme") ) {
        snprintf(css2, 1023, ".narrative { color: %s; background-color: %s; }"
                             ".thumbnail { background-color: %s; }", fgcol, bgcol, bgcol);
    }

    strcat(css1, css2);
    gtk_css_provider_load_from_string(provider, css1);
}


static void colloquium_startup(GApplication *papp)
{
    Colloquium *app = COLLOQUIUM(papp);
    GtkCssProvider *provider;

    G_APPLICATION_CLASS(colloquium_parent_class)->startup(papp);

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries,
                                     G_N_ELEMENTS(app_entries), app);

    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.new",
            (const char *[]){"<Control>n", NULL});
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.open",
            (const char *[]){"<Control>o", NULL});
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit",
            (const char *[]){"<Control>q", NULL});

    provider = gtk_css_provider_new();
    app->settings = g_settings_new("uk.me.bitwiz.colloquium");
    update_css(app->settings, NULL, provider);
    g_signal_connect(G_OBJECT(app->settings), "changed::narrative-fg",
                     G_CALLBACK(update_css), provider);
    g_signal_connect(G_OBJECT(app->settings), "changed::narrative-bg",
                     G_CALLBACK(update_css), provider);
    g_signal_connect(G_OBJECT(app->settings), "changed::narrative-use-theme",
                     G_CALLBACK(update_css), provider);
    g_signal_connect(G_OBJECT(app->settings), "changed::narrative-font",
                     G_CALLBACK(update_css), provider);

    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}


static Colloquium *colloquium_new()
{
    Colloquium *clq = g_object_new(COLLOQUIUM_TYPE_APP,
                                   "application-id", "uk.me.bitwiz.colloquium",
                                   "flags", G_APPLICATION_HANDLES_OPEN,
                                   NULL);
    return clq;
}


static void show_help(const char *s)
{
    printf(_("Syntax: %s [options] [<file.sc>]\n\n"), s);
    printf(_("Narrative-based presentation system.\n\n"
             "  -h, --help    Display this help message.\n"));
}


int main(int argc, char *argv[])
{
    int c;
    int status;
    Colloquium *app;

    /* Long options */
    const struct option longopts[] = {
        {"help",               0, NULL,               'h'},
        {0, 0, NULL, 0}
    };

    /* Short options */
    while ((c = getopt_long(argc, argv, "h", longopts, NULL)) != -1) {

        switch (c)
        {
            case 'h' :
            show_help(argv[0]);
            return 0;

            case 0 :
            break;

            default :
            return 1;
        }

    }

#if !GLIB_CHECK_VERSION(2,36,0)
    g_type_init();
#endif

    bindtextdomain("colloquium", LOCALEDIR);
    textdomain("colloquium");

    app = colloquium_new();
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
