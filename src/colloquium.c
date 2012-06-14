/*
 * colloquium.c
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

#include <gtk/gtk.h>
#include <getopt.h>


static void show_help(const char *s)
{
	printf("Syntax: %s [options]\n\n", s);
	printf(
"A tiny presentation program.\n"
"\n"
"  -h, --help                       Display this help message.\n"
"\n");
}


int main(int argc, char *argv[])
{
	int c;
	struct presentation *p;

	/* Long options */
	const struct option longopts[] = {
		{"help",               0, NULL,               'h'},
		{0, 0, NULL, 0}
	};

	gtk_init(&argc, &argv);

	/* Short options */
	while ((c = getopt_long(argc, argv, "h",
	                        longopts, NULL)) != -1) {

		switch (c) {
		case 'h' :
			show_help(argv[0]);
			return 0;

		case 0 :
			break;

		default :
			return 1;
		}

	}

//	p = new_presentation();
//	p->cur_edit_slide = add_slide(p, 0);
//	p->completely_empty = 1;
//	if ( open_mainwindow(p) ) {
//		fprintf(stderr, "Couldn't open main window.\n");
//		return 1;
//	}

	gtk_main();

	return 0;
}
