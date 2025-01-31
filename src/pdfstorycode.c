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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>

#include "storycode.h"
#include "narrative.h"
#include "slide.h"
#include "imagestore.h"
#include "slide_render_cairo.h"

#include <libintl.h>
#define _(x) gettext(x)


int main(int argc, char *argv[])
{
    GFile *file;
    GBytes *bytes;
    const char *text;
    size_t len;
    Narrative *n;
    ImageStore *is;

    file = g_file_new_for_commandline_arg(argv[1]);
    bytes = g_file_load_bytes(file, NULL, NULL, NULL);
    text = g_bytes_get_data(bytes, &len);
    n = storycode_parse_presentation(text);
    g_bytes_unref(bytes);

    is = imagestore_new(".");
    imagestore_set_parent(is, g_file_get_parent(file));

    /* Render each slide to PDF */
    render_slides_to_pdf(n, is, "slides.pdf");

    return 0;
}
