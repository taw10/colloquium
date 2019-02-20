/*
 * presentation.h
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

#ifndef PRESENTATION_H
#define PRESENTATION_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _presentation Presentation;

#include "stylesheet.h"
#include "narrative.h"

extern Presentation *presentation_new(void);
extern void presentation_free(Presentation *p);

extern void presentation_add_stylesheet(Presentation *p, Stylesheet *ss);
extern void presentation_add_narrative(Presentation *p, Narrative *n);
extern void presentation_add_slide(Presentation *p, Slide *s);

extern int presentation_num_slides(Presentation *p);
extern Slide *presentation_slide(Presentation *p, int i);

#endif /* PRESENTATION_H */
