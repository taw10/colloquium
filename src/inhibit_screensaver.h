/*
 * inhibit_screensaver.h
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

#ifndef INHIBIT_SCREENSAVER_H
#define INHIBIT_SCREENSAVER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct inhibit_sys;

extern void do_inhibit(struct inhibit_sys *sys, int flags);
extern void inhibit_cleanup(struct inhibit_sys *sys);
extern struct inhibit_sys *inhibit_prepare(void);

#endif	/* INHIBIT_SCREENSAVER_H */
