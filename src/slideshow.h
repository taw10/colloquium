/*
 * slideshow.h
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

#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#define SC_TYPE_SLIDESHOW            (sc_slideshow_get_type())

#define SC_SLIDESHOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                      SC_TYPE_SLIDESHOW, SCEditor))

#define SC_IS_SLIDESHOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                      SC_TYPE_SLIDESHOW))

#define SC_SLIDESHOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((obj), \
                                      SC_TYPE_SLIDESHOW, SCEditorClass))

#define SC_IS_SLIDESHOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((obj), \
                                      SC_TYPE_SLIDESHOW))

#define SC_SLIDESHOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
                                      SC_TYPE_SLIDESHOW, SCSlideShowClass))

struct _scslideshow
{
	GtkWindow parent_instance;

	/* <private> */
	Narrative           *n;
	Slide               *cur_slide;
	GtkWidget           *drawingarea;
	GdkCursor           *blank_cursor;
	int                  blank;
	int                  xoff;
	int                  yoff;
	int                  single_monitor;
	GtkApplication      *app;
	gint                 inhibit_cookie;
};


struct _scslideshowclass
{
	GtkWindowClass parent_class;
};

typedef struct _scslideshow SCSlideshow;
typedef struct _scslideshowclass SCSlideshowClass;

extern SCSlideshow *sc_slideshow_new(Narrative *n, GtkApplication *app);
extern void sc_slideshow_set_slide(SCSlideshow *ss, Slide *ns);
extern Slide *sc_slideshow_get_slide(SCSlideshow *ss);

#endif	/* SLIDESHOW_H */
