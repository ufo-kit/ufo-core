/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <ufo/ufo.h>
#include "test-suite.h"

typedef struct {
    UfoProfiler *profiler;
} Fixture;


static void
fixture_setup (Fixture *fixture, gconstpointer data)
{
    fixture->profiler = ufo_profiler_new ();
    g_assert (UFO_IS_PROFILER (fixture->profiler));
}

static void
fixture_teardown (Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->profiler);
}

static void
test_timer_elapsed (Fixture *fixture, gconstpointer data)
{
    gulong one_millisecond = G_USEC_PER_SEC / 1000;

    ufo_profiler_start (fixture->profiler,
                        UFO_PROFILER_TIMER_IO);

    g_usleep (one_millisecond);

    ufo_profiler_stop (fixture->profiler,
                       UFO_PROFILER_TIMER_IO);

    g_assert (ufo_profiler_elapsed (fixture->profiler,
                                    UFO_PROFILER_TIMER_CPU) <= 0.0);

    g_assert (ufo_profiler_elapsed (fixture->profiler,
                                    UFO_PROFILER_TIMER_IO) >= 0.001);
}


void
test_add_profiler (void)
{
    g_test_add ("/no-opencl/timer/elapsed",
                Fixture,
                NULL,
                fixture_setup,
                test_timer_elapsed,
                fixture_teardown);
}
