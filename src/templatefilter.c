/*
 * templatefilter.c
 *
 * Copyright © 2026 Thomas White <taw@bitwiz.org.uk>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "templatefilter.h"

static void converter_iface_init(GConverterIface *iface);

static GConverterResult convert_func(GConverter *conv,
                                     const void *inbuf, gsize inbuf_size,
                                     void *outbuf, gsize outbuf_size,
                                     GConverterFlags flags,
                                     gsize *pread, gsize *pwritten, GError **error);

G_DEFINE_FINAL_TYPE_WITH_CODE(TemplateFilter, colloquium_template_filter, G_TYPE_OBJECT,
                              G_IMPLEMENT_INTERFACE(G_TYPE_CONVERTER, converter_iface_init))


static void reset_func(GConverter *conv)
{
}


static void converter_iface_init(GConverterIface *iface)
{
    iface->convert = convert_func;
    iface->reset = reset_func;
}


static void colloquium_template_filter_class_init(TemplateFilterClass *klass)
{
}


static void colloquium_template_filter_init(TemplateFilter *e)
{
}


TemplateFilter *template_filter_new(const char *title)
{
    TemplateFilter *tf;

    tf = g_object_new(COLLOQUIUM_TYPE_TEMPLATE_FILTER, NULL);
    if ( title != NULL ) {
        tf->title = strdup(title);
    } else {
        tf->title = NULL;
    }

    return tf;
}


static GConverterResult convert_func(GConverter *conv,
                                     const void *inbuf, gsize inbuf_size,
                                     void *outbuf, gsize outbuf_size,
                                     GConverterFlags flags,
                                     gsize *pread, gsize *pwritten, GError **error)
{
    return G_CONVERTER_FINISHED;
}
