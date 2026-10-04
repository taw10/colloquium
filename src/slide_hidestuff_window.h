/*
 * slide_hidestuff_window.h
 *
 * Copyright © 2026 Thomas White <taw@bitwiz.me.uk>
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

#ifndef SLIDEHIDESTUFFWINDOW_H
#define SLIDEHIDESTUFFWINDOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _slidehidestuffwindow SlideHideStuffWindow;
typedef struct _slidehidestuffwindowclass SlideHideStuffWindowClass;

#include "narrative.h"

#define COLLOQUIUM_SLIDE_HIDESTUFFWINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                      COLLOQUIUM_TYPE_SLIDE_HIDESTUFFWINDOW, SlideHideStuffWindow))

#define COLLOQUIUM_TYPE_SLIDE_HIDESTUFFWINDOW (colloquium_slide_hidestuff_window_get_type())

struct _slidehidestuffwindow
{
    GtkWindow parent_instance;
};

struct _slidehidestuffwindowclass
{
    GtkWindowClass parent_class;
};

extern SlideHideStuffWindow *slide_hidestuff_window_new(Slide *s, Narrative *n);

#endif	/* SLIDEHIDESTUFFWINDOW_H */
