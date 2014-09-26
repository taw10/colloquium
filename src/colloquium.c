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

#include "presentation.h"
#include "narrative_window.h"


static void colloquium_activate(GApplication *app)
{
	printf("activate!\n");
}


static void colloquium_open(GApplication  *app, GFile **files, gint n_files,
                            const gchar *hint)
{
	int i;

	for ( i = 0; i<n_files; i++ ) {
		struct presentation *p;
		char *uri = g_file_get_path(files[i]);
		printf("open %s\n", uri);
		p = new_presentation();
		load_presentation(p, uri);
		narrative_window_new(p, app);
		g_free(uri);
	}
}

typedef struct
{
	GtkApplication parent_instance;
} Colloquium;

typedef GtkApplicationClass ColloquiumClass;

G_DEFINE_TYPE(Colloquium, colloquium, GTK_TYPE_APPLICATION)

static void colloquium_finalize(GObject *object)
{
	G_OBJECT_CLASS(colloquium_parent_class)->finalize(object);
}


static void colloquium_startup(GApplication *app)
{
	G_APPLICATION_CLASS(colloquium_parent_class)->startup(app);
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

	g_type_init();
	g_set_application_name("Colloquium");
	app = g_object_new(colloquium_get_type(),
	                   "application-id", "uk.org.bitwiz.Colloquium",
	                   "flags", G_APPLICATION_HANDLES_OPEN,
	                   "register-session", TRUE,
	                   NULL);
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

	app = colloquium_new();
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return status;
}
