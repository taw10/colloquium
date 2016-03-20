/*
 * boxvec.h
 *
 * Copyright © 2016 Thomas White <taw@bitwiz.org.uk>
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

#ifndef BOXVEC_H
#define BOXVEC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* A vector of boxes */
struct boxvec
{
	int n_boxes;
	int max_boxes;
	struct wrap_box **boxes;
};


/* Length of a boxvec */
extern int bv_len(struct boxvec *vec);

/* Create a bxvec or boxvec */
extern struct boxvec *bv_new(void);

/* Ensure space in a bxvec or boxvec */
extern int bv_ensure_space(struct boxvec *vec, int n);

/* Add to a bxvec or boxvec */
extern int bv_add(struct boxvec *vec, struct wrap_box *bx);

/* (Find and then) delete a box from a boxvec */
extern void bv_del(struct boxvec *vec, struct wrap_box *bx);

/* Add a new box after the specified one */
extern int bv_add_after(struct boxvec *vec, struct wrap_box *bx,
                        struct wrap_box *add);

/* Get a box from a boxvec or bxvec */
extern struct wrap_box *bv_box(struct boxvec *vec, int i);
extern struct wrap_box *bv_last(struct boxvec *vec);

/* Free a bxvec or boxvec */
extern void bv_free(struct boxvec *vec);

#endif /* BOXVEC_H */
