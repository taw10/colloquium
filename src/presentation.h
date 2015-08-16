/*
 * presentation.h
 *
 * Copyright © 2013-2014 Thomas White <taw@bitwiz.org.uk>
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

#ifndef PRESENTATION_H
#define PRESENTATION_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cairo.h>
#include <gtk/gtk.h>

struct presentation;
struct slide;

#include "imagestore.h"
#include "sc_parse.h"
#include "slideshow.h"
#include "narrative_window.h"
#include "slide_window.h"

struct slide
{
	struct presentation *parent;
	struct slide_template *st;

	SCBlock *scblocks;
	SCBlock *notes;
};

struct menu_pl;

struct presentation
{
	char             *filename;
	char             *titlebar;  /* basename(filename) or "(untitled)" */
	int               completely_empty;

	ImageStore       *is;

	NarrativeWindow  *narrative_window;
	SlideWindow      *slidewindow;

	struct notes     *notes;
	struct pr_clock  *clock;
	struct slide_sorter *slide_sorter;

	/* This is the "native" size of the slide.  It only exists to give
	 * font size some meaning in the context of a somewhat arbitrary DPI */
	double            slide_width;
	double            slide_height;

	unsigned int      num_slides;
	struct slide    **slides;

	SCBlock          *stylesheet;
	SCBlock          *scblocks;

};


extern struct presentation *new_presentation(void);
extern void free_presentation(struct presentation *p);

extern char *get_titlebar_string(struct presentation *p);

extern struct slide *new_slide(void);
extern struct slide *add_slide(struct presentation *p, int pos);
extern int insert_slide(struct presentation *p, struct slide *s, int pos);
extern void free_slide(struct slide *s);
extern void delete_slide(struct presentation *p, struct slide *s);

extern int slide_number(struct presentation *p, struct slide *s);

extern int load_presentation(struct presentation *p, const char *filename);
extern int save_presentation(struct presentation *p, const char *filename);

extern void set_edit(struct presentation *p, struct slide *s);

#define UNUSED __attribute__((unused))


#endif	/* PRESENTATION_H */
