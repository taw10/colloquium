/*
 * storycode_test.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-glib/json-glib.h>

#include "../src/sc_parse.h"
#include "../src/stylesheet.h"


int main(int argc, char *argv[])
{
	Stylesheet *ss;
	GFile *ssfile;

	ssfile = g_file_new_for_uri("resource:///uk/me/bitwiz/Colloquium/default.ss");
	ss = stylesheet_load(ssfile);
	printf("Frame bgcol: '%s'\n", stylesheet_lookup(ss, "$.slide.frame", "bgcol"));
	printf("Frame font: '%s'\n", stylesheet_lookup(ss, "$.slide.frame", "font"));
	printf("Narrative fgcol: '%s'\n", stylesheet_lookup(ss, "$.narrative", "fgcol"));

	return 0;
}
