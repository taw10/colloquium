/*
 * file_types.c
 *
 * Copyright © 2016-2026 Thomas White <taw@bitwiz.org.uk>
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <libintl.h>
#define _(x) gettext(x)

#include <gio/gio.h>
#include <glib.h>

#include "file_types.h"


enum slide_filetype query_file_type(GFile *file)
{
    GFileInfo *info;
    const char *type;
    GError *error;
    enum slide_filetype file_type;

    error = NULL;
    info = g_file_query_info(file, "standard::", G_FILE_QUERY_INFO_NONE, NULL, &error);
    if ( info == NULL ) {
        fprintf(stderr, _("Failed to read info: %s\n"), error->message);
        return SLIDE_FTYPE_UNKNOWN;
    }

    type = g_file_info_get_content_type(info);

    /* PDF types */
    if ( g_content_type_equals(type, "application/pdf") ) {
        file_type = SLIDE_FTYPE_PDF;
    } else if ( g_content_type_equals(type, "com.adobe.pdf") ) {
        file_type = SLIDE_FTYPE_PDF;

    /* Bitmap images */
    } else if ( g_content_type_equals(type, "public.png") ) {
        file_type = SLIDE_FTYPE_IMAGE;
    } else if ( g_content_type_equals(type, "image/jpeg") ) {
        file_type = SLIDE_FTYPE_IMAGE;
    } else if ( g_content_type_equals(type, "public.jpeg") ) {
        file_type = SLIDE_FTYPE_IMAGE;
    } else if ( g_content_type_equals(type, "image/png") ) {
        file_type = SLIDE_FTYPE_IMAGE;

    /* Vector images */
    } else if ( g_content_type_equals(type, "image/svg+xml") ) {
        file_type = SLIDE_FTYPE_SVG;
    } else if ( g_content_type_equals(type, "public.svg-image") ) {
        file_type = SLIDE_FTYPE_SVG;

    /* Video types */
    } else if ( g_content_type_equals(type, "image/gif") ) {
        file_type = SLIDE_FTYPE_VIDEO;
    } else if ( g_content_type_equals(type, "com.compuserve.gif") ) {
        file_type = SLIDE_FTYPE_VIDEO;
    } else if ( g_content_type_equals(type, "video/mpeg") ) {
        file_type = SLIDE_FTYPE_VIDEO;
    } else if ( g_content_type_equals(type, "public.mpeg") ) {
        file_type = SLIDE_FTYPE_VIDEO;

    } else {
        fprintf(stderr, "File format not recognised: %s\n", type);
        file_type = SLIDE_FTYPE_UNKNOWN;
    }

    g_object_unref(G_OBJECT(info));

    return file_type;
}
