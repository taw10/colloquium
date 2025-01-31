/*
 * render_cairo_common.c
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <pango/pango.h>

#include "storycode.h"

int runs_to_pangolayout(PangoLayout *layout, struct text_run *runs, int n_runs)
{
    size_t total_len = 0;
    int i;
    char *text;
    PangoAttrList *attrs;

    attrs = pango_layout_get_attributes(layout);

    /* Work out length of all text in item (paragraph) */
    for ( i=0; i<n_runs; i++ ) {
        total_len += strlen(runs[i].text);
    }

    /* Allocate the complete text */
    text = malloc(total_len+1);
    if ( text == NULL ) {
        fprintf(stderr, "Couldn't allocate combined text (%lli)\n",
               (long long int)total_len);
        return 1;
    }

    /* Put all of the text together */
    text[0] = '\0';
    size_t pos = 0;
    for ( i=0; i<n_runs; i++ ) {

        PangoAttribute *attr = NULL;
        size_t run_len = strlen(runs[i].text);

        switch ( runs[i].type ) {

            case TEXT_RUN_NORMAL :
            break;

            case TEXT_RUN_BOLD:
            attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
            break;

            case TEXT_RUN_ITALIC:
            attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
            break;

            case TEXT_RUN_UNDERLINE:
            attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
            break;

        }

        if ( attr != NULL ) {
            attr->start_index = pos;
            attr->end_index = pos + run_len;
            pango_attr_list_insert(attrs, attr);
        }

        /* FIXME: Should check that each bit of text finishes on a character boundary */
        pos += run_len;
        strcat(text, runs[i].text);

    }
    pango_layout_set_text(layout, text, -1);

    return 0;
}


PangoAlignment to_pangoalignment(enum alignment align)
{
    switch ( align ) {
        case ALIGN_LEFT : return PANGO_ALIGN_LEFT;
        case ALIGN_RIGHT : return PANGO_ALIGN_RIGHT;
        case ALIGN_CENTER : return PANGO_ALIGN_CENTER;
        default: return PANGO_ALIGN_LEFT;
    }
}
