
#include <glib.h>
#include "ufo-filter.h"

static void test_filter_connect_correct(void)
{
    UfoFilter *source = g_object_new(UFO_TYPE_FILTER, NULL);
    UfoFilter *destination = g_object_new(UFO_TYPE_FILTER, NULL);

    GError *error = NULL;
    ufo_filter_register_output(source, "foo", 2);
    ufo_filter_register_input(destination, "bar", 2);
    ufo_filter_connect_to(source, destination, &error);
    g_assert(error == NULL);

    g_assert(ufo_filter_get_output_channel(source) != NULL);
    g_assert(ufo_filter_get_input_channel(destination) != NULL);

    g_object_unref(source);
    g_object_unref(destination);
}

static void test_filter_connect_invalid(void)
{
    UfoFilter *source = g_object_new(UFO_TYPE_FILTER, NULL);
    UfoFilter *destination = g_object_new(UFO_TYPE_FILTER, NULL);

    GError *error = NULL;
    ufo_filter_connect_to(source, destination, &error);
    g_assert(error != NULL);

    g_object_unref(source);
    g_object_unref(destination);
}

static void test_filter_connect_ndim_mismatch(void)
{
    UfoFilter *source = g_object_new(UFO_TYPE_FILTER, NULL);
    UfoFilter *destination = g_object_new(UFO_TYPE_FILTER, NULL);

    GError *error = NULL;
    ufo_filter_register_output(source, "foo", 2);
    ufo_filter_register_input(destination, "bar", 3);
    ufo_filter_connect_to(source, destination, &error);
    g_assert(error != NULL);

    g_object_unref(source);
    g_object_unref(destination);
}

static void test_filter_connect_wrong_name(void)
{
    UfoFilter *source = g_object_new(UFO_TYPE_FILTER, NULL);
    UfoFilter *destination = g_object_new(UFO_TYPE_FILTER, NULL);

    GError *error = NULL;
    ufo_filter_register_output(source, "foo", 2);
    ufo_filter_register_input(destination, "bar", 2);
    ufo_filter_connect_by_name(source, "foo", destination, "baz", &error);
    g_assert(error != NULL);

    g_object_unref(source);
    g_object_unref(destination);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add_func("/filter/connect/correct", test_filter_connect_correct);
    g_test_add_func("/filter/connect/invalid", test_filter_connect_invalid);
    g_test_add_func("/filter/connect/ndim-mismatch", test_filter_connect_ndim_mismatch);
    g_test_add_func("/filter/connect/wrong-name", test_filter_connect_wrong_name);
    g_test_run();

    return 0;
}
