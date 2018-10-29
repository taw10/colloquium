/*
 * stylesheet.h
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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

#ifndef STYLESHEET_H
#define STYLESHEET_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <gdk/gdk.h>

typedef struct _stylesheet Stylesheet;

extern Stylesheet *stylesheet_load(GFile *file);

extern int stylesheet_save(Stylesheet *ss, GFile *file);

extern int parse_colour_duo(const char *a, GdkRGBA *col1, GdkRGBA *col2);

extern char *stylesheet_lookup(Stylesheet *ss, const char *path, const char *key);

extern int stylesheet_set(Stylesheet *ss, const char *path, const char *key,
                          const char *new_val);

extern int stylesheet_delete(Stylesheet *ss, const char *path, const char *key);

extern void stylesheet_free(Stylesheet *ss);

#endif	/* STYLESHEET_H */
