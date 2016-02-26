/*
 * boxvec.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "boxvec.h"


int bv_len(struct boxvec *vec)
{
	return vec->n_boxes;
}


extern struct boxvec *bv_new()
{
	struct boxvec *n = malloc(sizeof(struct boxvec));
	if ( n == NULL ) return NULL;

	n->n_boxes = 0;
	n->max_boxes = 0;
	n->boxes = NULL;

	return n;
}


int bv_ensure_space(struct boxvec *vec, int n)
{
	struct wrap_box **t;
	if ( vec == NULL ) return 1;
	if ( vec->max_boxes > n ) return 0;

	n = (n/32)*32 + 32;
	t = realloc(vec->boxes, n*sizeof(struct wrap_box *));
	if ( t == NULL ) return 1;

	vec->boxes = t;
	return 0;
}


int bv_add(struct boxvec *vec, struct wrap_box *bx)
{
	if ( vec == NULL ) return 1;
	if ( bv_ensure_space(vec, vec->n_boxes+1) ) return 1;
	vec->boxes[vec->n_boxes++] = bx;
	return 0;
}


struct wrap_box *bv_box(struct boxvec *vec, int i)
{
	assert(vec != NULL);
	if ( i >= vec->n_boxes ) return NULL;
	return vec->boxes[i];
}


struct wrap_box *bv_last(struct boxvec *vec)
{
	return vec->boxes[vec->n_boxes-1];
}


void bv_free(struct boxvec *vec)
{
	if ( vec == NULL ) return;
	free(vec->boxes);
	free(vec);
}

