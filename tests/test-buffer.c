/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <ufo/ufo.h>
#include "test-suite.h"

typedef struct {
    UfoBuffer *buffer;
    guint n_data;
    const guint8 *data8;
    const guint16 *data16;
} Fixture;

static void
setup (Fixture *fixture, gconstpointer data)
{
    static const guint8 data8[8] = { 1, 2, 1, 3, 1, 255, 1, 254 };
    static const guint16 data16[8] = { 1, 2, 1, 3, 1, 65535, 1, 65534 };

    UfoRequisition requisition = {
        .n_dims = 1,
        .dims[0] = 8,
    };

    fixture->buffer = ufo_buffer_new (&requisition, NULL, NULL);
    fixture->data8 = data8;
    fixture->data16 = data16;
    fixture->n_data = 8;
}

static void
teardown (Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->buffer);
}

static void
test_create_lots_of_buffers (Fixture *fixture,
                             gconstpointer unused)
{
    gint num_buffers = 10000;
    UfoConfig *config = ufo_config_new ();
    UfoResources *resources = ufo_resources_new (config, NULL);
    gpointer context = ufo_resources_get_context (resources);

    UfoRequisition requisition = {
        .n_dims = 2,
        .dims[0] = 800,
        .dims[1] = 800
    };

    while (num_buffers-- > 0) {
        UfoBuffer *buffer = ufo_buffer_new (&requisition, NULL, context);
        gpointer device_array = ufo_buffer_get_device_array (buffer, NULL);
        ufo_buffer_discard_location (buffer);
        g_assert (device_array != NULL);
        g_object_unref (buffer);
    }
}

static void
test_copy_lots_of_buffers (Fixture *fixture,
                           gconstpointer *unused)
{
    gint num_buffers = 10000;
    UfoConfig *config = ufo_config_new ();
    UfoResources *resources = ufo_resources_new (config, NULL);
    gpointer context = ufo_resources_get_context (resources);

    UfoRequisition requisition = {
        .n_dims = 2,
        .dims[0] = 800,
        .dims[1] = 800
    };

    // TODO enforce copy between GPUs
    UfoBuffer *src = ufo_buffer_new (&requisition, NULL, context);
    UfoBuffer *dest = ufo_buffer_new (&requisition, NULL, context);

    while (num_buffers-- > 0) {
        g_debug("copy");
        ufo_buffer_copy (src, dest);
    }

}
static void
test_convert_8 (Fixture *fixture,
                gconstpointer unused)
{
    gfloat *host_data;

    host_data = ufo_buffer_get_host_array (fixture->buffer, NULL);
    g_memmove (host_data, fixture->data8, fixture->n_data);

    ufo_buffer_convert (fixture->buffer, UFO_BUFFER_DEPTH_8U);
    host_data = ufo_buffer_get_host_array (fixture->buffer, NULL);

    for (guint i = 0; i < fixture->n_data; i++)
        g_assert (host_data[i] == ((gfloat) fixture->data8[i]));
}

static void
test_convert_8_from_data (Fixture *fixture,
                          gconstpointer unused)
{
    gfloat *host_data;

    ufo_buffer_convert_from_data (fixture->buffer, fixture->data8, UFO_BUFFER_DEPTH_8U);
    host_data = ufo_buffer_get_host_array (fixture->buffer, NULL);

    for (guint i = 0; i < fixture->n_data; i++)
        g_assert (host_data[i] == ((gfloat) fixture->data8[i]));
}

static void
test_convert_16 (Fixture *fixture,
                 gconstpointer unused)
{
    gfloat *host_data;

    host_data = ufo_buffer_get_host_array (fixture->buffer, NULL);
    g_memmove (host_data, fixture->data16, fixture->n_data * sizeof (guint16));

    ufo_buffer_convert (fixture->buffer, UFO_BUFFER_DEPTH_16U);
    host_data = ufo_buffer_get_host_array (fixture->buffer, NULL);

    for (guint i = 0; i < fixture->n_data; i++)
        g_assert (host_data[i] == ((gfloat) fixture->data16[i]));
}

static void
test_convert_16_from_data (Fixture *fixture,
                           gconstpointer unused)
{
    gfloat *host_data;

    ufo_buffer_convert_from_data (fixture->buffer, fixture->data16, UFO_BUFFER_DEPTH_16U);
    host_data = ufo_buffer_get_host_array (fixture->buffer, NULL);

    for (guint i = 0; i < fixture->n_data; i++)
        g_assert (host_data[i] == ((gfloat) fixture->data16[i]));
}

void
test_add_buffer (void)
{
    g_test_add ("/buffer/create",
                Fixture, NULL,
                setup, test_create_lots_of_buffers, teardown);
    g_test_add ("/buffer/convert/8/host",
                Fixture, NULL,
                setup, test_convert_8, teardown);

    g_test_add ("/no-opencl/buffer/convert/8/data",
                Fixture, NULL,
                setup, test_convert_8_from_data, teardown);

    g_test_add ("/no-opencl/buffer/convert/16/host",
                Fixture, NULL,
                setup, test_convert_16, teardown);

    g_test_add ("/no-opencl/buffer/convert/16/data",
                Fixture, NULL,
                setup, test_convert_16_from_data, teardown);
}
