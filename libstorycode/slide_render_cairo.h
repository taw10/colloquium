/*
 * render.h
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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

#ifndef RENDER_H
#define RENDER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "presentation.h"
#include "imagestore.h"

extern int slide_render_cairo(Slide *s, cairo_t *cr, ImageStore *is, Stylesheet *stylesheet,
                              int slide_number, PangoLanguage *lang,
                              PangoContext *pc);

#endif	/* RENDER_H */