#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include "test-suite.h"
#include "ufo-profiler.h"

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
                        UFO_PROFILER_TIMER_RELEASE_INPUT);

    g_usleep (one_millisecond);

    ufo_profiler_stop (fixture->profiler,
                       UFO_PROFILER_TIMER_RELEASE_INPUT);

    g_assert (ufo_profiler_elapsed (fixture->profiler,
                                    UFO_PROFILER_TIMER_FETCH_INPUT) <= 0.0);
    g_assert (ufo_profiler_elapsed (fixture->profiler,
                                    UFO_PROFILER_TIMER_FETCH_OUTPUT) <= 0.0);
    g_assert (ufo_profiler_elapsed (fixture->profiler,
                                    UFO_PROFILER_TIMER_RELEASE_OUTPUT) <= 0.0);

    g_assert (ufo_profiler_elapsed (fixture->profiler,
                                    UFO_PROFILER_TIMER_RELEASE_INPUT) >= 0.001);
}


void
test_add_profiler (void)
{
    g_test_add ("/timer/elapsed",
                Fixture,
                NULL,
                fixture_setup,
                test_timer_elapsed,
                fixture_teardown);
}
