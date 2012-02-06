
#include <glib.h>
#include "ufo-filter.h"

typedef struct {
    UfoFilter *source;
    UfoFilter *destination;
} UfoFilterFixture;

static void ufo_filter_fixture_setup(UfoFilterFixture *fixture, gconstpointer data)
{
    fixture->source = g_object_new(UFO_TYPE_FILTER, NULL);
    fixture->destination = g_object_new(UFO_TYPE_FILTER, NULL);
    g_assert(fixture->source);
    g_assert(fixture->destination);
}

static void ufo_filter_fixture_teardown(UfoFilterFixture *fixture, gconstpointer data)
{
    g_assert(fixture->source);
    g_assert(fixture->destination);
    g_object_unref(fixture->source);
    g_object_unref(fixture->destination);
}

static void test_filter_connect_correct(UfoFilterFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    ufo_filter_register_output(fixture->source, "foo", 2);
    ufo_filter_register_input(fixture->destination, "bar", 2);
    ufo_filter_connect_to(fixture->source, fixture->destination, &error);
    g_assert(error == NULL);

    g_assert(ufo_filter_get_output_channel(fixture->source) != NULL);
    g_assert(ufo_filter_get_input_channel(fixture->destination) != NULL);

    g_assert(ufo_filter_get_output_channel_by_name(fixture->source, "foo") != NULL);
    g_assert(ufo_filter_get_input_channel_by_name(fixture->destination, "bar") != NULL);
}

static void test_filter_connect_invalid(UfoFilterFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    ufo_filter_connect_to(fixture->source, fixture->destination, &error);
    g_assert(error != NULL);
    g_assert(error->code == UFO_FILTER_ERROR_INSUFFICIENTINPUTS || error->code == UFO_FILTER_ERROR_INSUFFICIENTOUTPUTS);
}

static void test_filter_connect_ndim_mismatch(UfoFilterFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    ufo_filter_register_output(fixture->source, "foo", 2);
    ufo_filter_register_input(fixture->destination, "bar", 3);
    ufo_filter_connect_to(fixture->source, fixture->destination, &error);
    g_assert(error != NULL);
}

static void test_filter_connect_wrong_name(UfoFilterFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    ufo_filter_register_output(fixture->source, "foo", 2);
    ufo_filter_register_input(fixture->destination, "bar", 2);
    ufo_filter_connect_by_name(fixture->source, "foo", fixture->destination, "baz", &error);
    g_assert(error != NULL);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add("/filter/connect/correct", UfoFilterFixture, NULL, ufo_filter_fixture_setup, 
            test_filter_connect_correct, ufo_filter_fixture_teardown);
    g_test_add("/filter/connect/invalid", UfoFilterFixture, NULL, ufo_filter_fixture_setup,
            test_filter_connect_invalid, ufo_filter_fixture_teardown);
    g_test_add("/filter/connect/ndim-mismatch", UfoFilterFixture, NULL, ufo_filter_fixture_setup,
            test_filter_connect_ndim_mismatch, ufo_filter_fixture_teardown);
    g_test_add("/filter/connect/wrong-name", UfoFilterFixture, NULL, ufo_filter_fixture_setup,
            test_filter_connect_wrong_name, ufo_filter_fixture_teardown);

    return g_test_run();
}
