/*
 * narrative_window.h
 *
 * Copyright © 2014-2017 Thomas White <taw@bitwiz.org.uk>
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

#ifndef NARRATIVE_WINDOW_H
#define NARRATIVE_WINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


typedef struct _narrative_window NarrativeWindow;

extern NarrativeWindow *narrative_window_new(struct presentation *p,
                                             GApplication *app);

extern void update_titlebar(NarrativeWindow *nw);

#endif	/* NARRATIVE_WINDOW_H */
