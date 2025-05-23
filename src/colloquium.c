/*
 * colloquium.c
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

struct _colloquium
{
    GtkApplication parent_instance;
    GtkBuilder *builder;
    char *mydir;
    int first_run;
    int hidepointer;
};


typedef GtkApplicationClass ColloquiumClass;


G_DEFINE_TYPE(Colloquium, colloquium, GTK_TYPE_APPLICATION)


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
        "Thomas White <taw@bitwiz.org.uk>",
        NULL
    };

    window = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(parent));

    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(window),
        "Colloquium");
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(window),
        "colloquium");
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


static void colloquium_finalize(GObject *object)
{
    G_OBJECT_CLASS(colloquium_parent_class)->finalize(object);
}


static void create_config(const char *filename)
{

    FILE *fh;
    fh = fopen(filename, "w");
    if ( fh == NULL ) {
        fprintf(stderr, _("Failed to create config\n"));
        return;
    }

    fprintf(fh, "hidepointer: no\n");

    fclose(fh);
}


static int yesno(const char *a)
{
    if ( a == NULL ) return 0;

    if ( strcmp(a, "1") == 0 ) return 1;
    if ( strcasecmp(a, "yes") == 0 ) return 1;
    if ( strcasecmp(a, "true") == 0 ) return 1;

    if ( strcasecmp(a, "0") == 0 ) return 0;
    if ( strcasecmp(a, "no") == 0 ) return 0;
    if ( strcasecmp(a, "false") == 0 ) return 0;

    fprintf(stderr, "Don't understand '%s', assuming false\n", a);
    return 0;
}


static void chomp(char *s)
{
    size_t i;

    if ( !s ) return;

    for ( i=0; i<strlen(s); i++ ) {
        if ( (s[i] == '\n') || (s[i] == '\r') ) {
            s[i] = '\0';
            return;
        }
    }
}


static void read_config(const char *filename, Colloquium *app)
{
    FILE *fh;
    char line[1024];

    fh = fopen(filename, "r");
    if ( fh == NULL ) {
        fprintf(stderr, _("Failed to open config %s\n"), filename);
        return;
    }

    do {

        if ( fgets(line, 1024, fh) == NULL ) break;
        chomp(line);

        if ( strncmp(line, "hidepointer: ", 12) == 0 ) {
            app->hidepointer = yesno(line+13);
        }
    } while ( !feof(fh) );

    fclose(fh);
}


int colloquium_get_hidepointer(Colloquium *app)
{
    return app->hidepointer;
}


static void colloquium_startup(GApplication *papp)
{
    Colloquium *app = COLLOQUIUM(papp);
    const char *configdir;
    char *tmp;

    G_APPLICATION_CLASS(colloquium_parent_class)->startup(papp);

    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries,
                                     G_N_ELEMENTS(app_entries), app);

    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.new",
            (const char *[]){"<Control>n", NULL});
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.open",
            (const char *[]){"<Control>o", NULL});
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit",
            (const char *[]){"<Control>q", NULL});

    configdir = g_get_user_config_dir();
    app->mydir = malloc(strlen(configdir)+14);
    strcpy(app->mydir, configdir);
    strcat(app->mydir, "/colloquium");

    if ( !g_file_test(app->mydir, G_FILE_TEST_IS_DIR) ) {

        /* Folder not created yet */
        open_intro_doc(app);
        app->first_run = 1;

        if ( g_mkdir(app->mydir, S_IRUSR | S_IWUSR | S_IXUSR) ) {
            fprintf(stderr, _("Failed to create config folder\n"));
        }
    }

    /* Read config file */
    tmp = malloc(strlen(app->mydir)+32);
    if ( tmp != NULL ) {

        tmp[0] = '\0';
        strcat(tmp, app->mydir);
        strcat(tmp, "/config");

        /* Create default config file if it doesn't exist */
        if ( !g_file_test(tmp, G_FILE_TEST_EXISTS) ) {
            create_config(tmp);
        }

        read_config(tmp, app);
        free(tmp);
    }
}


static void colloquium_shutdown(GApplication *app)
{
    G_APPLICATION_CLASS(colloquium_parent_class)->shutdown(app);
}


static void colloquium_class_init(ColloquiumClass *class)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS(class);
    GObjectClass *object_class = G_OBJECT_CLASS(class);

    app_class->startup = colloquium_startup;
    app_class->shutdown = colloquium_shutdown;
    app_class->activate = colloquium_activate;
    app_class->open = colloquium_open;

    object_class->finalize = colloquium_finalize;
}


static void colloquium_init(Colloquium *app)
{
    app->hidepointer = 0;
}


static Colloquium *colloquium_new()
{
    Colloquium *app;

    g_set_application_name("Colloquium");
    app = g_object_new(colloquium_get_type(),
                       "application-id", "uk.me.bitwiz.colloquium",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       "register-session", TRUE,
                       NULL);

    app->first_run = 0;  /* Will be updated at "startup" if appropriate */

    return app;
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
