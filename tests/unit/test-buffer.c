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
#include <math.h>
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

    fixture->buffer = ufo_buffer_new (&requisition, NULL, 0, 0);
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

static void
test_insert_metadata (Fixture *fixture,
                      gconstpointer unused)
{
    GValue value = {0};
    GValue *other;

    g_assert (ufo_buffer_get_metadata (fixture->buffer, "bar") == NULL);

    /* Insert data */
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, -123);

    ufo_buffer_set_metadata (fixture->buffer, "foo", &value);
    other = ufo_buffer_get_metadata (fixture->buffer, "foo");

    g_assert (other != NULL);
    g_assert (g_value_get_int (other) == -123);

    /* Overwrite data */
    g_value_unset (&value);
    g_value_init (&value, G_TYPE_FLOAT);
    g_value_set_float (&value, 3.14f);
    ufo_buffer_set_metadata (fixture->buffer, "foo", &value);

    other = ufo_buffer_get_metadata (fixture->buffer, "foo");
    g_assert (other != NULL);
    g_assert (fabs (g_value_get_float (other) - 3.14f) < 0.0001f);
}

static void
test_copy_metadata (Fixture *fixture,
                    gconstpointer unused)
{
    GValue value = {0};
    GValue *other;
    UfoBuffer *copy;

    UfoRequisition requisition = {
        .n_dims = 2,
        .dims[0] = 8,
        .dims[1] = 8,
    };

    copy = ufo_buffer_new (&requisition, NULL, 0, 0);

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, -123);

    ufo_buffer_set_metadata (fixture->buffer, "foo", &value);
    ufo_buffer_copy_metadata (fixture->buffer, copy);
    other = ufo_buffer_get_metadata (copy, "foo");
    g_assert (g_value_get_int (other) == -123);

    g_object_unref (copy);
}

static void
test_location (Fixture *fixture,
               gconstpointer unused)
{
    g_assert (ufo_buffer_get_location (fixture->buffer) == UFO_BUFFER_LOCATION_INVALID);
    ufo_buffer_get_host_array (fixture->buffer, NULL);
    g_assert (ufo_buffer_get_location (fixture->buffer) == UFO_BUFFER_LOCATION_HOST);
}

void
test_add_buffer (void)
{
    g_test_add ("/no-opencl/buffer/convert/8/host",
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

    g_test_add ("/no-opencl/buffer/metadata/insert",
                Fixture, NULL,
                setup, test_insert_metadata, teardown);

    g_test_add ("/no-opencl/buffer/metadata/copy",
                Fixture, NULL,
                setup, test_copy_metadata, teardown);

    g_test_add ("/no-opencl/buffer/location",
                Fixture, NULL,
                setup, test_location, teardown);
}
