/*
 * slide_pdf.h
 *
 * Copyright © 2019-2026 Thomas White <taw@bitwiz.me.uk>
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

#ifndef SLIDE_PDF_H
#define SLIDE_PDF_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <gio/gio.h>
#include <gdk/gdk.h>

extern float get_aspect_pdf(GFile *file, int pagenum);
extern GdkTexture *load_pdf(GFile *file, int pagenum, int w, cairo_t *in_cr);

#endif /* SLIDE_PDF_H */
