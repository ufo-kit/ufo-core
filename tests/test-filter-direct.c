#include <glib.h>
#include "test-suite.h"
#include "ufo-graph.h"
#include "ufo-buffer.h"
#include "ufo-filter-source-direct.h"
#include "ufo-filter-sink-direct.h"

typedef struct {
    UfoFilterSourceDirect *source;
    UfoFilterSinkDirect *sink;
    UfoBuffer *input;
} Fixture;

static const guint DIM_SIZE[] = {16, 16};

static const gfloat BUFFER_VALUE = 1.245f;

static void
fixture_setup (Fixture *fixture, gconstpointer data)
{

    fixture->source = g_object_new (UFO_TYPE_FILTER_SOURCE_DIRECT, NULL);
    g_assert (fixture->source);

    fixture->sink = g_object_new (UFO_TYPE_FILTER_SINK_DIRECT, NULL);
    g_assert (fixture->sink);

    fixture->input = ufo_buffer_new (2, DIM_SIZE);
    g_assert (fixture->input);

    ufo_buffer_fill_with_value (fixture->input, BUFFER_VALUE);
}

static void
fixture_teardown(Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->source);
    g_object_unref (fixture->sink);
    g_object_unref (fixture->input);
}

static gpointer
push_input (Fixture *fixture)
{
    ufo_filter_source_direct_push (fixture->source, fixture->input);
    ufo_filter_source_direct_stop (fixture->source);

    return NULL;
}

static gpointer
pop_output (Fixture *fixture)
{
    UfoBuffer *buffer;
    gfloat *data;

    buffer = ufo_filter_sink_direct_pop (fixture->sink);
    data = ufo_buffer_get_host_array (buffer, NULL);
    g_assert (data[0] == BUFFER_VALUE);
    ufo_filter_sink_direct_release (fixture->sink, buffer);

    return NULL;
}

static gpointer
scheduler (Fixture *fixture)
{
    UfoBuffer *buffer;
    GError *error = NULL;

    buffer = ufo_buffer_new (2, DIM_SIZE);
    ufo_buffer_fill_with_value (buffer, 9.0);

    /* Fake the scheduler by inserting one buffer between the filters and
     * calling the respective functions. */
    ufo_filter_source_generate (UFO_FILTER_SOURCE (fixture->source), &buffer, &error);
    g_assert_no_error (error);
    ufo_filter_sink_consume (UFO_FILTER_SINK (fixture->sink), &buffer, &error);
    g_assert_no_error (error);

    return NULL;
}

static void
test_filter_direct_process (Fixture *fixture, gconstpointer data)
{
    UfoGraph *graph;
    GThread *push_thread;
    GThread *pop_thread;
    GThread *scheduler_thread;
    GError *error = NULL;

    graph = ufo_graph_new ();
    ufo_graph_connect_filters (graph,
                               UFO_FILTER (fixture->source),
                               UFO_FILTER (fixture->sink),
                               &error);
    g_assert_no_error (error);

    scheduler_thread = g_thread_create ((GThreadFunc) scheduler,
                                   fixture, TRUE, &error);

    push_thread = g_thread_create ((GThreadFunc) push_input,
                                   fixture, TRUE, &error);
    g_assert_no_error (error);

    pop_thread = g_thread_create ((GThreadFunc) pop_output,
                                   fixture, TRUE, &error);
    g_assert_no_error (error);

    g_thread_join (push_thread);
    g_thread_join (pop_thread);
    g_thread_join (scheduler_thread);

    g_object_unref (graph);
}

void
test_add_filter_direct (void)
{
    g_test_add("/filter/direct", Fixture, NULL, fixture_setup, test_filter_direct_process, fixture_teardown);
}
