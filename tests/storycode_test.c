/*
 * storycode_test.c
 *
 * Colloquium - A tiny presentation program
 *
 * Copyright (c) 2012 Thomas White <taw@bitwiz.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
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

#include <stdio.h>

#include "../src/storycode.h"


static int test_sc(const char *tt)
{
	SCBlockList *bl;
	SCBlockListIterator *iter;
	struct scblock *b;

	printf("'%s' ->\n", tt);
	bl = sc_find_blocks(tt, "bg");

	if ( bl == NULL ) {
		printf("Failed to find blocks.\n");
		return 1;
	}

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		printf("   \\%s [%s] {%s}\n", b->name, b->options, b->contents);
	}
	sc_block_list_free(bl);

	printf("->\n");
	bl = sc_find_blocks(tt, NULL);

	if ( bl == NULL ) {
		printf("Failed to find blocks.\n");
		return 1;
	}

	for ( b = sc_block_list_first(bl, &iter);
	      b != NULL;
	      b = sc_block_list_next(bl, iter) )
	{
		printf("   \\%s [%s] {%s}\n", b->name, b->options, b->contents);
	}
	sc_block_list_free(bl);

	return 0;
}

int main(int argc, char *argv[])
{
	int v = 0;
	int r;

	r = test_sc("\\bg[a=b]{wibble \\f{wobble}}\\bg{rwawr}\\muhu Wobble"
	            "\\wibble{}\\f{wibble \\bg[muhu]{wobble}}\\frib[\\f] f");
	if ( r ) v = 1;

	r = test_sc("A B C \\wibble");
	if ( r ) v = 1;

	return v;
}
