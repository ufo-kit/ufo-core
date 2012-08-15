#include <glib.h>
#include "test-suite.h"
#include "ufo-filter.h"
#include "ufo-relation.h"

typedef struct {
    UfoFilter *producer;
    UfoFilter *consumer;
    UfoRelation *relation;
} UfoRelationFixture;

static void
ufo_relation_fixture_setup (UfoRelationFixture *fixture, gconstpointer data)
{
    fixture->producer = g_object_new (UFO_TYPE_FILTER, NULL);
    fixture->consumer = g_object_new (UFO_TYPE_FILTER, NULL);
    fixture->relation = ufo_relation_new (fixture->producer, 0, UFO_RELATION_MODE_DISTRIBUTE);
    g_assert (fixture->relation);
}

static void
ufo_relation_fixture_teardown (UfoRelationFixture *fixture, gconstpointer data)
{
    g_assert (fixture->relation);
    g_object_unref (fixture->relation);
    g_object_unref (fixture->producer);
    g_object_unref (fixture->consumer);
}

static void
test_relation_dim_match (UfoRelationFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT }};
    UfoOutputParameter output_params[] = {{2}};

    ufo_filter_register_inputs (fixture->consumer, 1, input_params);
    ufo_filter_register_outputs (fixture->producer, 1, output_params);
    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 0, &error);
    g_assert_no_error (error);
}

static void
test_relation_dim_mismatch (UfoRelationFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UfoInputParameter input_params[] = {{1, UFO_FILTER_INFINITE_INPUT }};
    UfoOutputParameter output_params[] = {{3}};

    ufo_filter_register_inputs (fixture->consumer, 1, input_params);
    ufo_filter_register_outputs (fixture->producer, 1, output_params);
    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 0, &error);
    g_assert (error != NULL);
}

static void
test_relation_invalid_input_port (UfoRelationFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT }};
    UfoOutputParameter output_params[] = {{2}};

    ufo_filter_register_inputs (fixture->consumer, 1, input_params);
    ufo_filter_register_outputs (fixture->producer, 1, output_params);
    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 1, &error);
    g_assert (error != NULL);
}

static void
test_relation_multi_output (UfoRelationFixture *fixture, gconstpointer data)
{
    UfoFilter   *consumer2;
    GAsyncQueue *push_queue = NULL;
    GAsyncQueue *pop_queue = NULL;
    GError      *error = NULL;

    UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT }};
    UfoOutputParameter output_params[] = {{2}};

    consumer2 = g_object_new (UFO_TYPE_FILTER, NULL);

    ufo_filter_register_inputs (fixture->consumer, 1, input_params);
    ufo_filter_register_inputs (consumer2, 1, input_params);
    ufo_filter_register_outputs (fixture->producer, 1, output_params);

    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 0, &error);
    ufo_relation_add_consumer (fixture->relation, consumer2, 0, &error);
    g_assert_no_error (error);

    ufo_relation_get_consumer_queues (fixture->relation,
                                      fixture->consumer,
                                      &push_queue,
                                      &pop_queue);

    g_assert (push_queue != NULL && pop_queue != NULL);

    ufo_relation_get_consumer_queues (fixture->relation,
                                      consumer2,
                                      &push_queue,
                                      &pop_queue);

    g_assert (push_queue != NULL && pop_queue != NULL);

    g_object_unref (consumer2);
}

void
test_add_relation (void)
{
    g_test_add("/relation/dim/match", UfoRelationFixture, NULL, ufo_relation_fixture_setup,
            test_relation_dim_match, ufo_relation_fixture_teardown);

    g_test_add("/relation/dim/mismatch", UfoRelationFixture, NULL, ufo_relation_fixture_setup,
            test_relation_dim_mismatch, ufo_relation_fixture_teardown);

    g_test_add("/relation/input/invalid", UfoRelationFixture, NULL, ufo_relation_fixture_setup,
            test_relation_invalid_input_port, ufo_relation_fixture_teardown);

    g_test_add("/relation/input/multi", UfoRelationFixture, NULL, ufo_relation_fixture_setup,
            test_relation_multi_output, ufo_relation_fixture_teardown);
}
