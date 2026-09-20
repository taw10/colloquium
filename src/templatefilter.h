/*
 * templatefilter.h
 *
 * Copyright © 2026 Thomas White <taw@bitwiz.me.uk>
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

#ifndef COLLOQUIUM_TEMPLATE_FILTER_H
#define COLLOQUIUM_TEMPLATE_FILTER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

typedef struct _templatefilter TemplateFilter;
typedef struct _templatefilterclass TemplateFilterClass;

#define COLLOQUIUM_TYPE_TEMPLATE_FILTER (colloquium_template_filter_get_type())

#define COLLOQUIUM_TEMPLATE_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                             COLLOQUIUM_TYPE_TEMPLATE_FILTER, TemplateFilter))

struct _templatefilter
{
    GObject              parent_instance;

    /*< private >*/
    char                *title;
};

struct _templatefilterclass
{
    GObjectClass parent_class;
};

extern GType colloquium_template_filter_get_type(void);

extern TemplateFilter *template_filter_new(const char *title);

#endif  /* COLLOQUIUM_TEMPLATE_FILTER_H */
