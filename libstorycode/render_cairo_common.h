/*
 * render_cairo_common.h
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

#ifndef RENDER_CAIRO_COMMON_H
#define RENDER_CAIRO_COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pango/pango.h>

#include "storycode.h"

extern int runs_to_pangolayout(PangoLayout *layout, struct text_run *runs, int n_runs);

#endif	/* RENDER_CAIRO_COMMON_H */
