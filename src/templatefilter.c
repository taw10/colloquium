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
                                     const void *inbufv, gsize inbuf_size,
                                     void *outbuf, gsize outbuf_size,
                                     GConverterFlags flags,
                                     gsize *pread, gsize *pwritten, GError **error)
{
    TemplateFilter *tf = COLLOQUIUM_TEMPLATE_FILTER(conv);
    gsize pos = 0;
    gsize npos = 0;
    const char *needle = "##TITLE##";
    gsize nlen = strlen(needle);
    gsize found_offs = 0;
    gsize nothing = 0;
    const char *inbuf = inbufv;

    while ( pos < inbuf_size ) {
        if ( inbuf[pos] == needle[npos] ) {
            npos++;
            if ( npos == nlen ) {
                found_offs = pos - nlen + 1;
                npos = 0;
                if ( found_offs == 0 ) {
                    /* Found at start */
                    if ( outbuf_size < nlen ) {
                        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE, "No space");
                        return G_CONVERTER_ERROR;;
                    }
                    memcpy(outbuf, tf->title, strlen(tf->title));
                    *pread = nlen;
                    *pwritten = strlen(tf->title);
                    return G_CONVERTER_CONVERTED;
                } else {
                    /* Found in the middle somewhere.
                     * Pass the bytes up to the start of the match, then handle
                     * the substitution (now at the start) on the next call. */
                    if ( outbuf_size < found_offs ) {
                        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE, "No space");
                        return G_CONVERTER_ERROR;;
                    }
                    memcpy(outbuf, inbuf, found_offs);
                    *pread = found_offs;
                    *pwritten = found_offs;
                    return G_CONVERTER_CONVERTED;
                }
            }
        } else {
            nothing += npos + 1;
            npos = 0;
        }
        pos++;
    }

    if ( (nothing == 0) && (npos > 0) && !(flags & G_CONVERTER_INPUT_AT_END) ) {
        /* Match begins at the start and doesn't stop matching by the end
         * of the input.  But doesn't match completely.  Need more input to
         * find out if this becomes a real match or not. */
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT, "Need more input");
        return G_CONVERTER_ERROR;
    }

    /* Pass through everything up to the start of a possible match */
    if ( outbuf_size < nothing ) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE, "No space");
        return G_CONVERTER_ERROR;;
    }
    memcpy(outbuf, inbuf, nothing);
    *pread = nothing;
    *pwritten = nothing;
    if ( flags & G_CONVERTER_INPUT_AT_END ) {
        return G_CONVERTER_FINISHED;
    } else {
        return G_CONVERTER_CONVERTED;
    }
}
