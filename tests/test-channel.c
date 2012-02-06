
#include <glib.h>
#include "ufo-channel.h"

typedef struct {
    UfoChannel *channel;
} UfoChannelFixture;

static void ufo_channel_fixture_setup(UfoChannelFixture *fixture, gconstpointer data)
{
    fixture->channel = ufo_channel_new();
    g_assert(fixture->channel);
}

static void ufo_channel_fixture_teardown(UfoChannelFixture *fixture, gconstpointer data)
{
    g_assert(fixture->channel);
    g_object_unref(fixture->channel);
}

static void test_channel_allocate_simple(UfoChannelFixture *fixture, gconstpointer data)
{
    const guint original_num_dims = 2;
    const guint original_dim_size[] = { 133, 209 };
    ufo_channel_allocate_output_buffers(fixture->channel, original_num_dims, original_dim_size);

    UfoBuffer *buffer = ufo_channel_get_output_buffer(fixture->channel);
    g_assert(buffer != NULL);

    guint new_num_dims = 0;
    guint *new_dim_size = NULL;
    ufo_buffer_get_dimensions(buffer, &new_num_dims, &new_dim_size);

    g_assert(new_num_dims == original_num_dims);
    g_assert(new_dim_size != NULL);

    for (guint i = 0; i < new_num_dims; i++)
        g_assert(new_dim_size[i] == original_dim_size[i]);

    g_free(new_dim_size);
}

static void test_channel_allocate_implicitly(UfoChannelFixture *fixture, gconstpointer data)
{
    const guint original_num_dims = 2;
    const guint original_dim_size[] = { 307, 482 };

    UfoBuffer *dummy = ufo_buffer_new(original_num_dims, original_dim_size);
    ufo_channel_allocate_output_buffers_like(fixture->channel, dummy);
    g_object_unref(dummy);

    UfoBuffer *buffer = ufo_channel_get_output_buffer(fixture->channel);
    g_assert(buffer != NULL);

    guint new_num_dims = 0;
    guint *new_dim_size = NULL;
    ufo_buffer_get_dimensions(buffer, &new_num_dims, &new_dim_size);

    g_assert(new_num_dims == original_num_dims);
    g_assert(new_dim_size != NULL);

    for (guint i = 0; i < new_num_dims; i++)
        g_assert(new_dim_size[i] == original_dim_size[i]);

    g_free(new_dim_size);
}

static void null_log_handler(const gchar *log_dman, GLogLevelFlags log_leve, const gchar *message, gpointer user_data)
{
}

int main(int argc, char *argv[])
{
    g_type_init();
    g_test_init(&argc, &argv, NULL);
    g_test_bug_base("http://ufo.kit.edu/ufo/ticket");
    g_log_set_handler("Ufo", G_LOG_LEVEL_DEBUG, null_log_handler, NULL);

    g_test_add("/channel/allocate/simple", UfoChannelFixture, NULL, ufo_channel_fixture_setup, 
            test_channel_allocate_simple, ufo_channel_fixture_teardown);
    g_test_add("/channel/allocate/implicitly", UfoChannelFixture, NULL, ufo_channel_fixture_setup, 
            test_channel_allocate_implicitly, ufo_channel_fixture_teardown);

    return g_test_run();
}
