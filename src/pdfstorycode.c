/*
 * pdfstorycode.c
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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


#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "storycode.h"
#include "presentation.h"

int main(int argc, char *argv[])
{
	GFile *file;
	GBytes *bytes;
	const char *text;
	size_t len;
	Presentation *p;
	int i;

	file = g_file_new_for_commandline_arg(argv[1]);
	bytes = g_file_load_bytes(file, NULL, NULL, NULL);
	text = g_bytes_get_data(bytes, &len);
	p = storycode_parse_presentation(text);
	g_bytes_unref(bytes);

	/* Render each slide to PDF */
//	for ( i=0; i<presentation_num_slides(p); i++ ) {
//		Slide *slide = presentation_slide(p, i);
//
//	}

	return 0;
}
