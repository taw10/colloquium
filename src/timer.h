/*
 * timer.h
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

#ifndef COLLOQUIUM_TIMER_H
#define COLLOQUIUM_TIMER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

typedef struct _colloquiumtimer Timer;
typedef struct _colloquiumtimerclass TimerClass;

#define COLLOQUIUM_TYPE_TIMER (colloquium_timer_get_type())

#define COLLOQUIUM_TIMER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                               COLLOQUIUM_TYPE_TIMER, Timer))

typedef enum {
    TIMER_MAIN,
    TIMER_DISCUSSION
} TimerMode;

struct _colloquiumtimer
{
    GObject            parent_instance;

    /*< private >*/
    GTimeZone *tz;

    GDateTime *start;
    double time_elapsed_at_start;
    TimerMode running_mode;

    double main_time;
    double discussion_time;

    double elapsed_main_time;
    double elapsed_discussion_time;

    int progress;           /* Number of words said so far */
    int max_progress;       /* High water mark for 'progress' */
    int progress_target;    /* Number of words to be said by end of presentation */
};

struct _colloquiumtimerclass
{
    GObjectClass parent_class;
};

extern GType colloquium_timer_get_type(void);

extern Timer *colloquium_timer_new(void);

extern void colloquium_timer_set_main_time(Timer *t, double seconds);
extern void colloquium_timer_set_discussion_time(Timer *t, double seconds);
extern void colloquium_timer_set_progress_target(Timer *t, int pos);

extern double colloquium_timer_get_main_time(Timer *t);
extern double colloquium_timer_get_discussion_time(Timer *t);

extern void colloquium_timer_start(Timer *t, TimerMode m);
extern void colloquium_timer_pause(Timer *t);
extern void colloquium_timer_reset(Timer *t);

extern void colloquium_timer_reset_max_progress(Timer *t);
extern void colloquium_timer_set_progress(Timer *t, int pos);

extern double colloquium_timer_get_elapsed_main_time(Timer *t);
extern double colloquium_timer_get_elapsed_discussion_time(Timer *t);
extern double colloquium_timer_get_progress_fraction(Timer *t);
extern double colloquium_timer_get_max_progress_fraction(Timer *t);
extern int colloquium_timer_get_running(Timer *t);
extern TimerMode colloquium_timer_get_running_mode(Timer *t);

#endif  /* COLLOQUIUM_TIMER_H */
