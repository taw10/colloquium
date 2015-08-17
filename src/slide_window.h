/*
 * slide_window.h
 *
 * Copyright © 2013-2015 Thomas White <taw@bitwiz.org.uk>
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

#ifndef SLIDEWINDOW_H
#define SLIDEWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _slidewindow SlideWindow;

extern SlideWindow *slide_window_open(struct presentation *p, GApplication *app);
extern void change_edit_slide(SlideWindow *sw, struct slide *np);
extern void update_titlebar(struct presentation *p);

extern void slidewindow_notes_closed(SlideWindow *sw);

#endif	/* SLIDEWINDOW_H */
