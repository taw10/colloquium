/*
 * sc_interp.h
 *
 * Copyright © 2014 Thomas White <taw@bitwiz.org.uk>
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

#ifndef SC_INTERP_H
#define SC_INTERP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _scinterp SCInterpreter;

extern SCInterpreter *sc_interp_new(void);
extern void sc_interp_destroy(SCInterpreter *scin);

extern void sc_interp_save(SCInterpreter *scin);
extern void sc_interp_restore(SCInterpreter *scin);

extern void sc_interp_run(SCInterpreter *scin, const char *sc);

#endif	/* SC_INTERP_H */
