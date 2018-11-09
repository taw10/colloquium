/*
 * utils.h
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
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

#ifndef UTILS_H
#define UTILS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef enum
{
	UNITS_SLIDE,
	UNITS_FRAC
} LengthUnits;

extern void chomp(char *s);
extern int safe_strcmp(const char *a, const char *b);
extern int parse_double(const char *a, float v[2]);
extern int parse_tuple(const char *a, float v[4]);
extern int parse_dims(const char *opt, double *wp, double *hp,
                      LengthUnits *wup, LengthUnits *hup,
                      double *xp, double *yp);

#include <libintl.h>
#define _(x) gettext(x)

#endif /* UTILS_H */
