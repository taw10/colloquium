/*
 * gtknarrativeview.c
 *
 * Copyright © 2013-2019 Thomas White <taw@bitwiz.org.uk>
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
#include <gtk/gtk.h>
#include <assert.h>
#include <math.h>
#include <libintl.h>
#define _(x) gettext(x)

#include <narrative.h>

#include "narrative_priv.h"
#include "gtknarrativeview.h"


void draw_ruler(cairo_t *cr, GtkNarrativeView *e)
{
    int start_item, end_item;
    int i;
    double start_item_time = 0.0;
    double end_item_time = 0.0;
    double t;
    PangoLayout *layout;
    PangoFontDescription *fontdesc;

    /* Background */
    cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
    cairo_rectangle(cr, 0.0, 0.0, e->ruler_width, e->h);
    cairo_fill(cr);

    /* Which items are visible? */
    narrative_get_item_range(e->n, e->scroll_pos, e->scroll_pos+e->visible_height,
                             &start_item, &end_item);

    for ( i=0; i<start_item; i++ ) {
        start_item_time += e->n->items[i].estd_duration;
    }
    t = start_item_time;
    for ( i=start_item; i<=end_item; i++ ) {
        t += e->n->items[i].estd_duration;
    }
    end_item_time = t;

    layout = pango_layout_new(gtk_widget_get_pango_context(GTK_WIDGET(e)));
    fontdesc = pango_font_description_from_string("Sans 12");
    pango_layout_set_font_description(layout, fontdesc);

    for ( i=start_item_time; i<=end_item_time; i++ ) {

        char tmp[64];
        double y0, y1, y;
        double yitem = narrative_find_time_pos(e->n, i);
        y0 = narrative_get_item_y(e->n, yitem);
        y1 = narrative_get_item_y(e->n, yitem+1);
        if ( isinf(y1) ) break;  /* End of narrative is visible */
        y = y0 + (y1 - y0)*(yitem-trunc(yitem));
        cairo_move_to(cr, 0.0, y);
        cairo_line_to(cr, 20.0, y);
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_stroke(cr);

        snprintf(tmp, 63, _("%i min"), i);
        cairo_move_to(cr, 5.0, y+2.0);
        pango_layout_set_text(layout, tmp, -1);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        cairo_fill(cr);
    }

    g_object_unref(layout);
    pango_font_description_free(fontdesc);
}
