
#include <glib.h>
#include <math.h>
#include "ufo-buffer.h"

static gboolean float_eq(float n1, float n2)
{
    static const float epsilon = 0.000001;
    return (fabs(n1-n2) < epsilon);
}

static void test_buffer_new(void)
{
    const gint32 dimensions[] = { 1000, 1000, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_2D, dimensions);
    g_assert(buffer != NULL);
    g_object_unref(buffer);
}

/**
 * Check that data is correctly added to a UfoBuffer.
 */
static void test_buffer_set_data(void)
{
    const gint32 dimensions[] = { 10, 1, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_1D, dimensions);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
    ufo_buffer_set_cpu_data(buffer, test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);

    /* We can pass NULL since the buffer is never copied onto a GPU device */
    float *result = ufo_buffer_get_cpu_data(buffer, NULL);
    for (int i = 0; i < 10; i++)
        g_assert(float_eq(test_data[i], result[i]));
    g_object_unref(buffer);
}

static void test_buffer_set_too_much_data(void)
{
    const gint32 dimensions[] = { 1, 1, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_1D, dimensions);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0 };
    ufo_buffer_set_cpu_data(buffer, test_data, 2 * sizeof(float), &error);
    g_assert(error != NULL);
    g_object_unref(buffer);
}

/**
 * Check that data with different type than float is correctly converted.
 */
static void test_buffer_reinterpret_8bit(void)
{
    const gint32 dimensions[] = { 10, 1, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_1D, dimensions);

    GError *error = NULL;
    guint8 test_data[] = { 1, 2, 1, 3, 1, 4, 1, 5, 1, 6 };
    ufo_buffer_set_cpu_data(buffer, (float *) test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);
    ufo_buffer_reinterpret(buffer, 8, 10);

    float *result = ufo_buffer_get_cpu_data(buffer, NULL);
    g_assert(float_eq(result[0], 1 / 255.));
    g_assert(float_eq(result[1], 2 / 255.));
    g_object_unref(buffer);
}

static void test_buffer_reinterpret_16bit(void)
{
    const gint32 dimensions[] = { 10, 1, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_1D, dimensions);

    GError *error = NULL;
    guint16 test_data[] = { 1, 2, 1, 3, 1, 4, 1, 5, 1, 6 };
    ufo_buffer_set_cpu_data(buffer, (float *) test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);
    ufo_buffer_reinterpret(buffer, 16, 10);

    float *result = ufo_buffer_get_cpu_data(buffer, NULL);
    g_assert(float_eq(result[0], 1 / 65535.));
    g_assert(float_eq(result[1], 2 / 65535.));
    g_object_unref(buffer);
}

static void test_buffer_dimensions(void)
{
    const gint32 in_dimensions[] = { 123, 321, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_2D, in_dimensions);

    gint32 out_dimensions[4];
    ufo_buffer_get_dimensions(buffer, out_dimensions);

    for (int i = 0; i < 4; i++)
        g_assert_cmpuint(in_dimensions[i], ==, out_dimensions[i]);
    g_object_unref(buffer);
}

static void test_buffer_size(void)
{
    const gint32 in_dimensions[] = { 123, 321, 4, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_2D, in_dimensions);

    gsize num_bytes = ufo_buffer_get_size(buffer);
    g_assert_cmpuint(num_bytes, ==, 123 * 321 * 4 * sizeof(float));
    g_object_unref(buffer);
}

static void test_buffer_copy(void)
{
    const gint32 dimensions[] = { 5, 2, 1, 1 };
    UfoBuffer *buffer = ufo_buffer_new(UFO_BUFFER_2D, dimensions);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
    ufo_buffer_set_cpu_data(buffer, test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);

    UfoBuffer *copy = ufo_buffer_new(UFO_BUFFER_2D, dimensions);
    ufo_buffer_copy(buffer, copy, NULL);

    /* Check meta-data of copy */
    UfoStructure buffer_structure;
    UfoAccess buffer_access;
    UfoDomain buffer_domain;
    g_object_get(copy,
        "structure", &buffer_structure, 
        "access", &buffer_access,
        "domain", &buffer_domain,
        NULL);

    g_assert(buffer_structure == UFO_BUFFER_2D);

    /* Check contents of copy */
    float *result = ufo_buffer_get_cpu_data(copy, NULL);
    for (int i = 0; i < 10; i++)
        g_assert(float_eq(test_data[i], result[i]));

    g_object_unref(buffer);
    g_object_unref(copy);
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");

    g_test_add_func("/buffer/new", test_buffer_new);
    g_test_add_func("/buffer/dimensions", test_buffer_dimensions);
    g_test_add_func("/buffer/set-too-much-data", test_buffer_set_too_much_data);
    g_test_add_func("/buffer/set-data", test_buffer_set_data);
    g_test_add_func("/buffer/reinterpret/8bit", test_buffer_reinterpret_8bit);
    g_test_add_func("/buffer/reinterpret/16bit", test_buffer_reinterpret_16bit);
    g_test_add_func("/buffer/size", test_buffer_size);
    g_test_add_func("/buffer/copy", test_buffer_copy);
    g_test_run();

    return 0;
}
