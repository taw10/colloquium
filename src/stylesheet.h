/*
 * stylesheet.h
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
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

#ifndef STYLESHEET_H
#define STYLESHEET_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "loadsave.h"
#include "frame.h"


struct style
{
	char *name;

	struct layout_parameters lop;

	/* Storycode prologue (run through the interpreter before the
	 * main storycode for the frame */
	char    *sc_prologue;
};


struct slide_template
{
	char                 *name;

	/* References to the styles in the main list */
	struct style        **styles;
	int                   n_styles;
};


typedef struct _stylesheet StyleSheet;
struct presentation;

extern StyleSheet *new_stylesheet();
extern StyleSheet *load_stylesheet(const char *filename);
extern void free_stylesheet(StyleSheet *ss);
extern StyleSheet *default_stylesheet(void);

extern struct style *new_style(StyleSheet *ss, const char *name);
extern struct style *find_style(StyleSheet *ss, const char *name);

extern struct slide_template *new_template(StyleSheet *ss, const char *name);
extern void add_to_template(struct slide_template *t, struct style *sty);

extern int save_stylesheet(StyleSheet *ss, const char *filename);
extern StyleSheet *tree_to_stylesheet(struct ds_node *root);
extern void write_stylesheet(StyleSheet *ss, struct serializer *ser);

typedef struct _styleiterator StyleIterator;
extern struct style *style_first(StyleSheet *ss, StyleIterator **piter);
extern struct style *style_next(StyleSheet *ss, StyleIterator *iter);

typedef struct _templateiterator TemplateIterator;
extern struct slide_template *template_first(StyleSheet *ss,
                                             TemplateIterator **piter);
extern struct slide_template *template_next(StyleSheet *ss,
                                            TemplateIterator *iter);

#endif	/* STYLESHEET_H */
