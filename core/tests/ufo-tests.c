
#include <glib.h>
#include "ufo-graph.h"
#include "ufo-buffer.h"

static gboolean float_eq(float n1, float n2)
{
    static const float epsilon = 0.000001;
    return (abs(n1-n2) < epsilon);
}

static void test_graph_simple(void)
{
    UfoGraph *graph = NULL;
    g_assert(graph == NULL);
}

static void test_buffer_set_data(void)
{
    UfoBuffer *buffer = ufo_buffer_new(10, 1);
    g_assert(buffer != NULL);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
    ufo_buffer_set_cpu_data(buffer, test_data, 10, &error);
    g_assert(error == NULL);

    float *result = ufo_buffer_get_cpu_data(buffer);
    for (int i = 0; i < 10; i++)
        g_assert(float_eq(test_data[i], result[i]));
    g_object_unref(buffer);
}

static void test_buffer_set_too_much_data(void)
{
    UfoBuffer *buffer = ufo_buffer_new(1, 1);
    g_assert(buffer != NULL);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0 };
    ufo_buffer_set_cpu_data(buffer, test_data, 2, &error);
    g_assert(error != NULL);
    g_object_unref(buffer);
}

static void test_buffer_reinterpret(void)
{
    UfoBuffer *buffer = ufo_buffer_new(10, 1);
    g_assert(buffer != NULL);

    GError *error = NULL;
    guint8 test_data[] = { 1, 2, 1, 3, 1, 4, 1, 5, 1, 6 };
    ufo_buffer_set_cpu_data(buffer, (float *) test_data, 10, &error);
    g_assert(error == NULL);
    ufo_buffer_reinterpret(buffer, UFO_BUFFER_DEPTH_8, 10);

    float *result = ufo_buffer_get_cpu_data(buffer);
    g_assert(float_eq(result[0], 1 / 255.));
    g_assert(float_eq(result[1], 2 / 255.));
    g_object_unref(buffer);
}

static void test_buffer_dimensions(void)
{
    const int in_width = 123;
    const int in_height = 321;

    UfoBuffer *buffer = ufo_buffer_new(in_width, in_height);
    g_assert(buffer != NULL);

    gint32 out_width, out_height;
    ufo_buffer_get_dimensions(buffer, &out_width, &out_height);
    g_assert_cmpuint(in_width, ==, out_width);
    g_assert_cmpuint(in_height, ==, out_height);
    g_object_unref(buffer);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/graph/simple", test_graph_simple);

    g_test_add_func("/buffer/dimensions", test_buffer_dimensions);
    g_test_add_func("/buffer/set_too_much_data", test_buffer_set_too_much_data);
    g_test_add_func("/buffer/set_data", test_buffer_set_data);
    g_test_add_func("/buffer/reinterpret/8bit", test_buffer_reinterpret);

    g_test_run();
    return 0;
}
