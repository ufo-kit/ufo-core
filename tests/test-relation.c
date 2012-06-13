
#include <glib.h>
#include "ufo-filter.h"
#include "ufo-relation.h"

typedef struct {
    UfoFilter *producer;
    UfoFilter *consumer;
    UfoRelation *relation;
} UfoRelationFixture;

static void ufo_relation_fixture_setup(UfoRelationFixture *fixture, gconstpointer data)
{
    fixture->producer = g_object_new (UFO_TYPE_FILTER, NULL);
    fixture->consumer = g_object_new (UFO_TYPE_FILTER, NULL);
    fixture->relation = ufo_relation_new (fixture->producer, 0, UFO_RELATION_MODE_DISTRIBUTE);
    g_assert (fixture->relation);
}

static void ufo_relation_fixture_teardown(UfoRelationFixture *fixture, gconstpointer data)
{
    g_assert (fixture->relation);
    g_object_unref (fixture->relation);
    g_object_unref (fixture->producer);
    g_object_unref (fixture->consumer);
}

static void test_relation_dim_match(UfoRelationFixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_filter_register_outputs (fixture->producer, 2, NULL);
    ufo_filter_register_inputs (fixture->consumer, 2, NULL);
    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 0, &error);
    g_assert_no_error (error);
}

static void test_relation_dim_mismatch(UfoRelationFixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_filter_register_outputs (fixture->producer, 3, NULL);
    ufo_filter_register_inputs (fixture->consumer, 1, NULL);
    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 0, &error);
    g_assert (error != NULL);
}

static void test_relation_invalid_input_port(UfoRelationFixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_filter_register_outputs (fixture->producer, 2, NULL);
    ufo_filter_register_inputs (fixture->consumer, 2, NULL);
    ufo_relation_add_consumer (fixture->relation, fixture->consumer, 1, &error);
    g_assert (error != NULL);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add("/relation/dim/match", UfoRelationFixture, NULL, ufo_relation_fixture_setup, 
            test_relation_dim_match, ufo_relation_fixture_teardown);

    g_test_add("/relation/dim/mismatch", UfoRelationFixture, NULL, ufo_relation_fixture_setup, 
            test_relation_dim_mismatch, ufo_relation_fixture_teardown);

    g_test_add("/relation/input/invalid", UfoRelationFixture, NULL, ufo_relation_fixture_setup, 
            test_relation_invalid_input_port, ufo_relation_fixture_teardown);

    return g_test_run();
}
