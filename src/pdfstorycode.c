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
#include "presentation.h"
#include "slide.h"
#include "slide_render_cairo.h"

#include <libintl.h>
#define _(x) gettext(x)


static int render_slides_to_pdf(Presentation *p, const char *filename)
{
	double w = 2048.0;
	cairo_surface_t *surf;
	cairo_t *cr;
	int i;
	PangoContext *pc;

	surf = cairo_pdf_surface_create(filename, w, w);
	if ( cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS ) {
		fprintf(stderr, _("Couldn't create Cairo surface\n"));
		return 1;
	}

	cr = cairo_create(surf);
	pc = pango_cairo_create_context(cr);

	for ( i=0; i<presentation_num_slides(p); i++ )
	{
		Slide *s;
		double log_w, log_h;

		s = presentation_slide(p, i);
		slide_get_logical_size(s, &log_w, &log_h);

		cairo_pdf_surface_set_size(surf, w, w*(log_h/log_w));

		cairo_save(cr);
		cairo_scale(cr, w/log_w, w/log_w);
		slide_render_cairo(s, cr, presentation_get_stylesheet(p),
		                   i, pango_language_get_default(), pc);
		cairo_show_page(cr);
		cairo_restore(cr);
	}

	g_object_unref(pc);
	cairo_surface_finish(surf);
	cairo_destroy(cr);

	return 0;
}


int main(int argc, char *argv[])
{
	GFile *file;
	GBytes *bytes;
	const char *text;
	size_t len;
	Presentation *p;

	file = g_file_new_for_commandline_arg(argv[1]);
	bytes = g_file_load_bytes(file, NULL, NULL, NULL);
	text = g_bytes_get_data(bytes, &len);
	p = storycode_parse_presentation(text);
	g_bytes_unref(bytes);

	/* Render each slide to PDF */
	render_slides_to_pdf(p, "slides.pdf");

	return 0;
}
