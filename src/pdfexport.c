/*
 * pdfexport.c
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
#include <string.h>

#include <libintl.h>
#define _(x) gettext(x)

#include <gio/gio.h>
#include <cairo.h>
#include <cairo-pdf.h>

#include "narrative.h"
#include "slide.h"
#include "thumbnailwidget.h"


int export_pdf(Narrative *n, GFile *file)
{
    gchar *filename;
    cairo_surface_t *surf;
    GtkTextIter pos;
    cairo_t *cr;

    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(n->textbuf);
    GtkTextTag *slidetag = gtk_text_tag_table_lookup(table, "slide");

    gtk_text_buffer_get_start_iter(n->textbuf, &pos);

    /* Sadly no cairo_pdf_surface_create_for_gfile (yet?) */
    filename = g_file_get_path(file);
    if ( filename == NULL ) return 1;
    surf = cairo_pdf_surface_create(filename, 1, 1);
    g_free(filename);

    cr = cairo_create(surf);

    do {

        if ( gtk_text_iter_starts_tag(&pos, slidetag) ) {
            GtkTextChildAnchor *anc;
            anc = gtk_text_iter_get_child_anchor(&pos);
            if ( anc != NULL ) {
                Slide *slide;
                guint nc;
                GtkWidget **th = gtk_text_child_anchor_get_widgets(anc, &nc);
                assert(nc == 1);
                slide = thumbnail_get_slide(COLLOQUIUM_THUMBNAIL(th[0]));
                g_free(th);

                float asp = slide_get_aspect(slide);
                cairo_pdf_surface_set_size(surf, 1000, 1000/asp);
                cairo_save(cr);
                slide_render_cairo(slide, 1000, cr);
                cairo_restore(cr);
                cairo_show_page(cr);

            } else {
                fprintf(stderr, "Slide but no anchor found!\n");
            }
        }

    } while ( gtk_text_iter_forward_to_tag_toggle(&pos, slidetag) );

    cairo_destroy(cr);
    cairo_surface_finish(surf);

    return 0;
}
