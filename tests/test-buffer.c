
#include <glib.h>
#include <math.h>
#include "ufo-buffer.h"

static gboolean float_eq(float n1, float n2)
{
    static const gdouble epsilon = 0.000001;
    return (fabs(n1-n2) < epsilon);
}

static void test_buffer_new(void)
{
    const guint dimensions[] = { 1000, 1000 };
    UfoBuffer *buffer = ufo_buffer_new(2, dimensions);
    g_assert(buffer != NULL);
    g_object_unref(buffer);
}

/**
 * Check that data is correctly added to a UfoBuffer.
 */
static void test_buffer_set_data(void)
{
    const guint dimensions = 10;
    UfoBuffer *buffer = ufo_buffer_new(1, &dimensions);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
    ufo_buffer_set_host_array(buffer, test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);

    /* We can pass NULL since the buffer is never copied onto a GPU device */
    float *result = ufo_buffer_get_host_array(buffer, NULL);
    for (int i = 0; i < 10; i++)
        g_assert(float_eq(test_data[i], result[i]));

    g_object_unref(buffer);
}

static void test_buffer_set_too_much_data(void)
{
    const guint dimensions = 1;
    UfoBuffer *buffer = ufo_buffer_new(1, &dimensions);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0 };
    ufo_buffer_set_host_array(buffer, test_data, 2 * sizeof(float), &error);
    g_assert(error != NULL);
    g_object_unref(buffer);
}

/**
 * Check that data with different type than float is correctly converted.
 */
static void test_buffer_reinterpret_8bit(void)
{
    const guint dimensions = 10;
    UfoBuffer *buffer = ufo_buffer_new(1, &dimensions);

    GError *error = NULL;
    guint8 test_data[] = { 1, 2, 1, 3, 1, 4, 1, 5, 1, 6 };
    ufo_buffer_set_host_array(buffer, (float *) test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);
    ufo_buffer_reinterpret(buffer, 8, 10, TRUE);

    float *result = ufo_buffer_get_host_array(buffer, NULL);
    g_assert(float_eq(result[0], 1 / 255.0f));
    g_assert(float_eq(result[1], 2 / 255.0f));
    g_object_unref(buffer);
}

static void test_buffer_reinterpret_16bit(void)
{
    const guint dimensions = 10;
    UfoBuffer *buffer = ufo_buffer_new(1, &dimensions);

    GError *error = NULL;
    guint16 test_data[] = { 1, 2, 1, 3, 1, 4, 1, 5, 1, 6 };
    ufo_buffer_set_host_array(buffer, (float *) test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);
    ufo_buffer_reinterpret(buffer, 16, 10, TRUE);

    float *result = ufo_buffer_get_host_array(buffer, NULL);
    g_assert(float_eq(result[0], 1 / 65535.0f));
    g_assert(float_eq(result[1], 2 / 65535.0f));
    g_object_unref(buffer);
}

static void test_buffer_dimensions(void)
{
    const guint in_dimensions[] = { 123, 321 };
    UfoBuffer *buffer = ufo_buffer_new(2, in_dimensions);

    guint num_dims = 0;
    guint *out_dimensions = NULL;
    ufo_buffer_get_dimensions(buffer, &num_dims, &out_dimensions);
    g_assert(num_dims == 2);

    for (guint i = 0; i < num_dims; i++)
        g_assert_cmpuint(in_dimensions[i], ==, out_dimensions[i]);

    guint width = 0, height = 0;
    ufo_buffer_get_2d_dimensions(buffer, &width, &height);
    g_assert(width == in_dimensions[0]);
    g_assert(height == in_dimensions[1]);

    g_free(out_dimensions);
    g_object_unref(buffer);
}

static void test_buffer_size(void)
{
    const guint in_dimensions[] = { 123, 321, 4 };
    UfoBuffer *buffer = ufo_buffer_new(3, in_dimensions);

    gsize num_bytes = ufo_buffer_get_size(buffer);
    g_assert_cmpuint(num_bytes, ==, 123 * 321 * 4 * sizeof(float));
    g_object_unref(buffer);
}

static void test_buffer_copy(void)
{
    const guint dimensions[] = { 5, 2 };
    UfoBuffer *buffer = ufo_buffer_new(2, dimensions);

    GError *error = NULL;
    float test_data[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
    ufo_buffer_set_host_array(buffer, test_data, 10 * sizeof(float), &error);
    g_assert(error == NULL);

    UfoBuffer *copy = ufo_buffer_new(2, dimensions);
    ufo_buffer_copy(buffer, copy, NULL);

    float *result = ufo_buffer_get_host_array(copy, NULL);
    for (int i = 0; i < 10; i++)
        g_assert(float_eq(test_data[i], result[i]));

    g_object_unref(buffer);
    g_object_unref(copy);
}

static void test_buffer_swap(void)
{
    const guint dimensions[] = { 4, 4 };

    UfoBuffer *a = ufo_buffer_new(2, dimensions);
    UfoBuffer *b = ufo_buffer_new(2, dimensions);

    gfloat *h_a = ufo_buffer_get_host_array(a, NULL);
    gfloat *h_b = ufo_buffer_get_host_array(b, NULL);

    ufo_buffer_swap_host_arrays(a, b);

    gfloat *sh_a = ufo_buffer_get_host_array(a, NULL);
    gfloat *sh_b = ufo_buffer_get_host_array(b, NULL);

    g_assert(h_a == sh_b);
    g_assert(h_b == sh_a);

    g_object_unref(a);
    g_object_unref(b);
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
    g_test_add_func("/buffer/swap", test_buffer_swap);
    g_test_run();

    return 0;
}
