/*
 * narrative_render_cairo.h
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

#ifndef NARRATIVE_RENDER_CAIRO_H
#define NARRATIVE_RENDER_CAIRO_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "presentation.h"
#include "imagestore.h"


extern int narrative_wrap(Narrative *n, Stylesheet *stylesheet,
                          PangoLanguage *lang, PangoContext *pc, double w,
                          ImageStore *is);

extern double narrative_get_height(Narrative *n);

extern int narrative_render_cairo(Narrative *n, cairo_t *cr,
                                  Stylesheet *stylesheet);

#endif	/* NARRATIVE_RENDER_CAIRO_H */