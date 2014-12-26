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

#include <pango/pangocairo.h>

struct presentation;
typedef struct _scinterp SCInterpreter;
typedef struct _sccallbacklist SCCallbackList;
typedef cairo_surface_t *(*SCCallbackFunc)(SCInterpreter *scin, SCBlock *bl,
                                           void *);

extern SCInterpreter *sc_interp_new(PangoContext *pc, struct frame *top);
extern void sc_interp_destroy(SCInterpreter *scin);

extern void sc_interp_save(SCInterpreter *scin);
extern void sc_interp_restore(SCInterpreter *scin);

extern int sc_interp_add_blocks(SCInterpreter *scin, SCBlock *bl);

extern void find_stylesheet(struct presentation *p);
extern void sc_interp_run_stylesheet(SCInterpreter *scin, SCBlock *bl);
extern void add_macro(SCInterpreter *scin, const char *mname,
                      const char *contents);


/* Callback lists */
extern SCCallbackList *sc_callback_list_new();
extern void sc_callback_list_free(SCCallbackList *cbl);
extern void sc_callback_list_add_callback(SCCallbackList *cbl, const char *name,
                                          SCCallbackFunc func, void *vp);
extern void sc_interp_set_callbacks(SCInterpreter *scin, SCCallbackList *cbl);

/* Get the current state of the interpreter */
extern struct frame *sc_interp_get_frame(SCInterpreter *scin);
extern PangoFont *sc_interp_get_font(SCInterpreter *scin);
extern PangoFontDescription *sc_interp_get_fontdesc(SCInterpreter *scin);
extern double *sc_interp_get_fgcol(SCInterpreter *scin);
extern int sc_interp_get_ascent(SCInterpreter *scin);
extern int sc_interp_get_height(SCInterpreter *scin);
extern void update_geom(struct frame *fr);
extern SCBlock *sc_interp_get_macro_real_block(SCInterpreter *scin);


struct style_id
{
	char *name;
	char *friendlyname;
};

extern struct style_id *list_styles(SCInterpreter *scin, int *n);

#endif	/* SC_INTERP_H */
