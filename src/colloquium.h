/*
 * colloquium.h
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

#ifndef COLLOQUIUM_H
#define COLLOQUIUM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

typedef struct _colloquium Colloquium;
typedef struct _colloquiumclass ColloquiumClass;

#define COLLOQUIUM_TYPE_APP (colloquium_get_type())
#define COLLOQUIUM(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), COLLOQUIUM_TYPE_APP, Colloquium))

struct _colloquium
{
    GtkApplication parent_instance;

    /*< private >*/
    GtkBuilder *builder;
    char *mydir;
    int first_run;
    int hidepointer;
    GSettings *settings;
};

struct _colloquiumclass
{
    GtkApplicationClass parent_class;
};


extern GType colloquium_get_type(void);

extern void open_about_dialog(GtkWidget *parent);


#endif	/* COLLOQUIUM_H */
