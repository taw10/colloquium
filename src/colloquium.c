/*
 * colloquium.c
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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
#include <sys/stat.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "colloquium.h"
#include "presentation.h"
#include "narrative_window.h"


struct _colloquium
{
	GtkApplication parent_instance;
	char *mydir;
	int first_run;
};


typedef GtkApplicationClass ColloquiumClass;


G_DEFINE_TYPE(Colloquium, colloquium, GTK_TYPE_APPLICATION)


static void colloquium_activate(GApplication *papp)
{
	Colloquium *app = COLLOQUIUM(papp);
	if ( !app->first_run ) {
		struct presentation *p;
		p = new_presentation();
		narrative_window_new(p, papp);
	}
}


static void new_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GApplication *app = vp;
	g_application_activate(app);
}


static void about_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GtkWidget *window;

	const gchar *authors[] = {
		"Thomas White <taw@bitwiz.org.uk>",
		NULL
	};

	window = gtk_about_dialog_new();

	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(window),
	    "Colloquium");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(window),
	    PACKAGE_VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(window),
	    "© 2016 Thomas White <taw@bitwiz.org.uk>");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(window),
	    "Narrative-based presentation system");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(window),
	    "© 2016 Thomas White <taw@bitwiz.org.uk>\n");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(window),
	    "http://www.bitwiz.org.uk/");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(window), authors);

	g_signal_connect(window, "response", G_CALLBACK(gtk_widget_destroy),
	    NULL);

	gtk_widget_show_all(window);
}


static void quit_sig(GSimpleAction *action, GVariant *parameter, gpointer vp)
{
	GApplication *app = vp;
	g_application_quit(app);
}


GActionEntry app_entries[] = {

	{ "new", new_sig, NULL, NULL, NULL  },
	{ "about", about_sig, NULL, NULL, NULL  },
	{ "quit", quit_sig, NULL, NULL, NULL  },
};


static void colloquium_open(GApplication  *app, GFile **files, gint n_files,
                            const gchar *hint)
{
	int i;

	for ( i = 0; i<n_files; i++ ) {
		struct presentation *p;
		char *uri = g_file_get_path(files[i]);
		p = new_presentation();
		if ( load_presentation(p, uri) == 0 ) {
			narrative_window_new(p, app);
		} else {
			fprintf(stderr, "Failed to load '%s'\n", uri);
		}
		g_free(uri);
	}
}


static void colloquium_finalize(GObject *object)
{
	G_OBJECT_CLASS(colloquium_parent_class)->finalize(object);
}


static void colloquium_startup(GApplication *papp)
{
	Colloquium *app = COLLOQUIUM(papp);
	GtkBuilder *builder;
	const char *configdir;

	G_APPLICATION_CLASS(colloquium_parent_class)->startup(papp);

	g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries,
	                                 G_N_ELEMENTS(app_entries), app);

	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder,
	    "<interface>"

	    "  <menu id='app-menu'>"
	    "    <section>"
	    "      <item>"
	    "        <attribute name='label'>_New</attribute>"
	    "        <attribute name='action'>app.new</attribute>"
	    "        <attribute name='accel'>&lt;Primary&gt;n</attribute>"
	    "      </item>"
	    "      <item>"
	    "        <attribute name='label'>_Open</attribute>"
	    "        <attribute name='action'>app.open</attribute>"
	    "        <attribute name='accel'>&lt;Primary&gt;o</attribute>"
	    "      </item>"
	    "      <item>"
	    "        <attribute name='label'>Preferences...</attribute>"
	    "        <attribute name='action'>app.prefs</attribute>"
	    "      </item>"
	    "    </section>"
	    "    <section>"
	    "      <item>"
	    "        <attribute name='label'>_About</attribute>"
	    "        <attribute name='action'>app.about</attribute>"
	    "      </item>"
	    "      <item>"
	    "        <attribute name='label'>_Quit</attribute>"
	    "        <attribute name='action'>app.quit</attribute>"
	    "        <attribute name='accel'>&lt;Primary&gt;q</attribute>"
	    "      </item>"
	    "    </section>"
	    "  </menu>"

	    "  <menu id='menubar'>"
	    "    <submenu>"
	    "      <attribute name='label' translatable='yes'>File</attribute>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>_Save</attribute>"
	    "          <attribute name='action'>win.save</attribute>"
	    "          <attribute name='accel'>&lt;Primary&gt;s</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Save As...</attribute>"
	    "          <attribute name='action'>win.saveas</attribute>"
	    "        </item>"
	    "      </section>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Print...</attribute>"
	    "          <attribute name='action'>win.print</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Export slides as PDF...</attribute>"
	    "          <attribute name='action'>win.exportpdf</attribute>"
	    "        </item>"
	    "      </section>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Load stylesheet</attribute>"
	    "          <attribute name='action'>win.loadstyle</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Save stylesheet</attribute>"
	    "          <attribute name='action'>win.savestyle</attribute>"
	    "        </item>"
	    "      </section>"
	    "    </submenu>"

	    "    <submenu>"
	    "      <attribute name='label' translatable='yes'>Edit</attribute>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Undo</attribute>"
	    "          <attribute name='action'>win.undo</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Redo</attribute>"
	    "          <attribute name='action'>win.redo</attribute>"
	    "        </item>"
	    "      </section>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Cut</attribute>"
	    "          <attribute name='action'>win.cut</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Copy</attribute>"
	    "          <attribute name='action'>win.copy</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Paste</attribute>"
	    "          <attribute name='action'>win.paste</attribute>"
	    "        </item>"
	    "      </section>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Delete frame</attribute>"
	    "          <attribute name='action'>win.deleteframe</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Delete slide</attribute>"
	    "          <attribute name='action'>win.deleteslide</attribute>"
	    "        </item>"
	    "      </section>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Edit stylesheet...</attribute>"
	    "          <attribute name='action'>win.stylesheet</attribute>"
	    "        </item>"
	    "      </section>"
	    "    </submenu>"

	    "    <submenu>"
	    "      <attribute name='label' translatable='yes'>Insert</attribute>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Slide</attribute>"
	    "          <attribute name='action'>win.slide</attribute>"
	    "        </item>"
	    "      </section>"
	    "    </submenu>"

	    "    <submenu>"
	    "      <attribute name='label' translatable='yes'>Tools</attribute>"
	    "      <section>"
	    "        <item>"
	    "          <attribute name='label'>Start slideshow</attribute>"
	    "          <attribute name='action'>win.startslideshow</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Presentation clock...</attribute>"
	    "          <attribute name='action'>win.clock</attribute>"
	    "        </item>"
	    "        <item>"
	    "          <attribute name='label'>Test card</attribute>"
	    "          <attribute name='action'>win.testcard</attribute>"
	    "        </item>"
	    "      </section>"
	    "    </submenu>"
	    "  </menu>"

	    "</interface>", -1, NULL);

	gtk_application_set_app_menu(GTK_APPLICATION(app),
	    G_MENU_MODEL(gtk_builder_get_object(builder, "app-menu")));
	gtk_application_set_menubar(GTK_APPLICATION(app),
	    G_MENU_MODEL(gtk_builder_get_object(builder, "menubar")));
	g_object_unref(builder);

	configdir = g_get_user_config_dir();
	app->mydir = malloc(strlen(configdir+14));
	strcpy(app->mydir, configdir);
	strcat(app->mydir, "/colloquium");

	if ( !g_file_test(app->mydir, G_FILE_TEST_IS_DIR) ) {

		/* Folder not created yet */
		GFile *file = g_file_new_for_path(DATADIR"/colloquium/demo.sc");
		g_application_open(G_APPLICATION(app), &file, 1, "");
		app->first_run = 1;
		g_object_unref(file);

		if ( g_mkdir(app->mydir, S_IRUSR | S_IWUSR | S_IXUSR) ) {
			fprintf(stderr, "Failed to create folder\n");
		}
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
}


static Colloquium *colloquium_new()
{
	Colloquium *app;

	g_set_application_name("Colloquium");
	app = g_object_new(colloquium_get_type(),
	                   "application-id", "uk.org.bitwiz.Colloquium",
	                   "flags", G_APPLICATION_HANDLES_OPEN,
	                   "register-session", TRUE,
	                   NULL);

	app->first_run = 0;  /* Will be updated at "startup" if appropriate */

	return app;
}


static void show_help(const char *s)
{
	printf("Syntax: %s [options] [<file.sc>]\n\n", s);
	printf(
"A tiny presentation program.\n"
"\n"
"  -h, --help                       Display this help message.\n"
"\n");
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

	g_type_init();

	app = colloquium_new();
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return status;
}
