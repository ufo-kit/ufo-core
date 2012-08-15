
#include <glib.h>
#include <math.h>
#include "test-suite.h"
#include "ufo-channel.h"
#include "ufo-buffer.h"

static void
test_channel_new (void)
{
    UfoChannel *channel = ufo_channel_new ();
    g_assert (channel != NULL);
    g_object_unref (channel);
}

static void
test_channel_transport (void)
{
    UfoChannel *channel;
    UfoBuffer *buffer1;
    UfoBuffer *buffer2;
    UfoBuffer *output;
    UfoBuffer *input;
    guint dim_size = 256;

    buffer1 = ufo_buffer_new (1, &dim_size);
    buffer2 = ufo_buffer_new (1, &dim_size);

    channel = ufo_channel_new ();
    ufo_channel_insert (channel, buffer1);
    ufo_channel_insert (channel, buffer2);
    ufo_channel_ref (channel);

    output = ufo_channel_fetch_output (channel);
    g_assert (output == buffer1);

    ufo_channel_release_output (channel, output);

    output = ufo_channel_fetch_output (channel);
    g_assert (output == buffer2);

    /* It must be possible to receive buffer1 as input now */
    input = ufo_channel_fetch_input (channel);
    g_assert (input == buffer1);

    g_object_unref (input);
    g_object_unref (output);
    g_object_unref (channel);
}

static void
test_channel_finish (void)
{
    UfoChannel *channel;
    UfoBuffer *buffer;
    UfoBuffer *input;
    guint dim_size = 256;

    buffer = ufo_buffer_new (1, &dim_size);
    channel = ufo_channel_new ();
    ufo_channel_insert (channel, buffer);
    ufo_channel_ref (channel);

    ufo_channel_release_output (channel,
                                ufo_channel_fetch_output (channel));

    ufo_channel_finish (channel);

    input = ufo_channel_fetch_input (channel);
    g_assert (input == buffer);
    g_object_unref (input);

    input = ufo_channel_fetch_input (channel);
    g_assert (input == GINT_TO_POINTER (1));

    g_object_unref (channel);
}

void test_add_channel (void)
{
    g_test_add_func("/channel/new",
                    test_channel_new);

    g_test_add_func("/channel/transport",
                    test_channel_transport);

    g_test_add_func("/channel/finish",
                    test_channel_finish);
}
