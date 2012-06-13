
#include <glib.h>
#include "ufo-filter.h"

typedef struct {
    UfoFilter *filter;
} UfoFilterFixture;

static void ufo_filter_fixture_setup (UfoFilterFixture *fixture, gconstpointer data)
{
    fixture->filter = g_object_new (UFO_TYPE_FILTER, NULL);
    g_assert (fixture->filter);
}

static void ufo_filter_fixture_teardown(UfoFilterFixture *fixture, gconstpointer data)
{
    g_assert (fixture->filter);
    g_object_unref (fixture->filter);
}

static void test_filter_register_inputs (UfoFilterFixture *fixture, gconstpointer data)
{
    guint num_inputs;

    ufo_filter_register_inputs (fixture->filter, 2, 3, NULL);
    num_inputs = ufo_filter_get_num_inputs (fixture->filter);
    g_assert (num_inputs == 2);
}

static void test_filter_register_outputs (UfoFilterFixture *fixture, gconstpointer data)
{
    guint num_outputs;

    ufo_filter_register_outputs (fixture->filter, 2, 2, 2, NULL);
    num_outputs = ufo_filter_get_num_outputs (fixture->filter);
    g_assert (num_outputs == 3);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add("/filter/register/inputs", UfoFilterFixture, NULL, ufo_filter_fixture_setup, 
            test_filter_register_inputs, ufo_filter_fixture_teardown);

    g_test_add("/filter/register/outputs", UfoFilterFixture, NULL, ufo_filter_fixture_setup, 
            test_filter_register_outputs, ufo_filter_fixture_teardown);

    return g_test_run();
}
