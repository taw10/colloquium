/*
 * timer.c
 *
 * Copyright © 2025 Thomas White <taw@bitwiz.me.uk>
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
#include <glib-object.h>

#include "timer.h"

G_DEFINE_FINAL_TYPE(Timer, colloquium_timer, G_TYPE_OBJECT)


static void colloquium_timer_dispose(GObject *o)
{
    Timer *t = COLLOQUIUM_TIMER(o);
    if ( t->start != NULL ) g_date_time_unref(t->start);
    g_time_zone_unref(t->tz);
    t->start = NULL;
    t->tz = NULL;
    G_OBJECT_CLASS(colloquium_timer_parent_class)->dispose(o);
}


static void colloquium_timer_class_init(TimerClass *klass)
{
    GObjectClass *obj_class = G_OBJECT_CLASS(klass);
    obj_class->dispose = colloquium_timer_dispose;

    g_signal_new("clock-event", COLLOQUIUM_TYPE_TIMER,
                 G_SIGNAL_RUN_LAST, 0,
                 NULL, NULL, NULL, G_TYPE_NONE, 0);
}


static void colloquium_timer_init(Timer *self)
{
}


void colloquium_timer_set_progress(Timer *t, int progress)
{
    t->progress = progress;
    if ( t->progress > t->max_progress ) {
        t->max_progress = progress;
    }
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


void colloquium_timer_reset_max_progress(Timer *t)
{
    t->max_progress = t->progress;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


void colloquium_timer_reset(Timer *t)
{
    t->elapsed_main_time = 0;
    t->elapsed_discussion_time = 0;
    t->time_elapsed_at_start = 0;

    if ( t->start != NULL ) {
        g_date_time_unref(t->start);
    }
    t->start = NULL;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


void colloquium_timer_pause(Timer *t)
{
    GDateTime *dt;
    GTimeSpan sp;
    double time_passed;

    if ( t->start == NULL ) return;

    dt = g_date_time_new_now(t->tz);
    sp = g_date_time_difference(dt, t->start);
    time_passed = t->time_elapsed_at_start + sp / G_TIME_SPAN_SECOND;

    switch ( t->running_mode ) {

        case TIMER_MAIN:
        t->elapsed_main_time = time_passed;
        break;

        case TIMER_DISCUSSION:
        t->elapsed_discussion_time = time_passed;
        break;
    }

    g_date_time_unref(t->start);
    t->start = NULL;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


void colloquium_timer_start(Timer *t, TimerMode m)
{
    if ( (t->start != NULL) && (m == t->running_mode) ) return;
    colloquium_timer_pause(t);
    switch ( m ) {

        case TIMER_MAIN:
        t->time_elapsed_at_start = t->elapsed_main_time;
        break;

        case TIMER_DISCUSSION:
        t->time_elapsed_at_start = t->elapsed_discussion_time;
        break;
    }
    t->start = g_date_time_new_now(t->tz);
    t->running_mode = m;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


Timer *colloquium_timer_new()
{
    Timer *t;

    t = COLLOQUIUM_TIMER(g_object_new(COLLOQUIUM_TYPE_TIMER, NULL));
    t->tz = g_time_zone_new_local();

    t->main_time = 0;
    t->discussion_time = 0;
    t->elapsed_main_time = 0;
    t->elapsed_discussion_time = 0;
    t->time_elapsed_at_start = 0;
    t->progress = 0;
    t->max_progress = 0;
    t->progress_target = 1;
    t->start = NULL;

    return t;
}


double colloquium_timer_get_elapsed_main_time(Timer *t)
{
    GDateTime *dt;
    GTimeSpan sp;

    if ( t->start == NULL ) return t->elapsed_main_time;
    if ( t->running_mode != TIMER_MAIN ) return t->elapsed_main_time;

    dt = g_date_time_new_now(t->tz);
    sp = g_date_time_difference(dt, t->start);
    return t->time_elapsed_at_start + sp / G_TIME_SPAN_SECOND;
}


double colloquium_timer_get_elapsed_discussion_time(Timer *t)
{
    GDateTime *dt;
    GTimeSpan sp;

    if ( t->start == NULL ) return t->elapsed_discussion_time;
    if ( t->running_mode != TIMER_DISCUSSION ) return t->elapsed_discussion_time;

    dt = g_date_time_new_now(t->tz);
    sp = g_date_time_difference(dt, t->start);
    return t->time_elapsed_at_start + sp / G_TIME_SPAN_SECOND;
}


double colloquium_timer_get_progress_fraction(Timer *t)
{
    if ( t->progress >= t->progress_target ) return 1.0;
    if ( t->progress_target > 1 ) {
        return (double)t->progress / (t->progress_target-1);
    } else {
        return 1.0;
    }
}


double colloquium_timer_get_max_progress_fraction(Timer *t)
{
    if ( t->max_progress >= t->progress_target ) return 1.0;
    if ( t->progress_target > 1 ) {
        return (double)t->max_progress / (t->progress_target-1);
    } else {
        return 0.0;
    }
}


void colloquium_timer_set_main_time(Timer *t, double seconds)
{
    t->main_time = seconds;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


void colloquium_timer_set_discussion_time(Timer *t, double seconds)
{
    t->discussion_time = seconds;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


void colloquium_timer_set_progress_target(Timer *t, int pos)
{
    t->progress_target = pos;
    g_signal_emit_by_name(G_OBJECT(t), "clock-event");
}


double colloquium_timer_get_main_time(Timer *t)
{
    return t->main_time;
}


double colloquium_timer_get_discussion_time(Timer *t)
{
    return t->discussion_time;
}


int colloquium_timer_get_running(Timer *t)
{
    return t->start != NULL;
}


TimerMode colloquium_timer_get_running_mode(Timer *t)
{
    return t->running_mode;
}
