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
#include <stdio.h>
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
        tf->title = strdup("");
    }

    return tf;
}


static GConverterResult convert_func(GConverter *conv,
                                     const void *inbuf, gsize inbuf_size,
                                     void *outbuf, gsize outbuf_size,
                                     GConverterFlags flags,
                                     gsize *pread, gsize *pwritten, GError **error)
{
    TemplateFilter *tf = COLLOQUIUM_TEMPLATE_FILTER(conv);
    gchar *pos;

    if ( (inbuf_size < 9) && !(flags & G_CONVERTER_INPUT_AT_END) ) {
        return G_IO_ERROR_PARTIAL_INPUT;
    }

    printf("flags %i, in %lu, out %lu\n", flags, inbuf_size, outbuf_size);
    pos = g_strstr_len(inbuf, inbuf_size, "##TITLE##");

    if ( pos == inbuf ) {
        gsize tlen = strlen(tf->title);
        printf("got it at start\n");
        if ( outbuf_size < tlen ) return G_IO_ERROR_NO_SPACE;
        memcpy(outbuf, tf->title, strlen(tf->title));
        *pread = 9;
        *pwritten = strlen(tf->title);
        printf("out %lu, read %lu\n", *pwritten, *pread);
        return G_CONVERTER_CONVERTED;
    }

    if ( pos != NULL ) {
        gsize len = (void *)pos - inbuf;
        printf("got it at offset %lu\n", len);
        if ( outbuf_size < len ) return G_IO_ERROR_NO_SPACE;
        memcpy(outbuf, inbuf, len);
        *pread = len;
        *pwritten = len;
        printf("out %lu, read %lu\n", *pwritten, *pread);
        return G_CONVERTER_CONVERTED;
    }

    if ( flags & G_CONVERTER_INPUT_AT_END ) {
        if ( outbuf_size < inbuf_size ) return G_IO_ERROR_NO_SPACE;
        memcpy(outbuf, inbuf, inbuf_size);
        *pread = inbuf_size;
        *pwritten = inbuf_size;
        printf("finished out %lu, read %lu\n", *pwritten, *pread);
        return G_CONVERTER_FINISHED;
    }

    if ( outbuf_size < inbuf_size-0 ) return G_IO_ERROR_NO_SPACE;
    memcpy(outbuf, inbuf, inbuf_size-0);
    *pread = inbuf_size - 0;
    *pwritten = inbuf_size - 0;
    printf("out %lu, read %lu\n", *pwritten, *pread);
    return G_CONVERTER_CONVERTED;
}
