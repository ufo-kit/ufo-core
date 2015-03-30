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
#include <glib/gprintf.h>
#include <ufo/ufo.h>
#include "test-suite.h"

typedef struct {
    UfoDaemon *daemon;
    UfoRemoteNode *remote_node;
} Fixture;

static void
setup (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    const gchar *protocol = (const gchar *)data;
    gchar *addr = g_malloc (strlen ("://127.0.0.1:5555") + strlen (protocol));
    g_sprintf (addr, "%s://127.0.0.1:5555", protocol);
    fixture->daemon = ufo_daemon_new (addr);
    ufo_daemon_start (fixture->daemon, &error);
    g_assert_no_error (error);

    fixture->remote_node = (UfoRemoteNode *) ufo_remote_node_new (addr);

    g_free (addr);
}

static void
teardown (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    g_object_unref (fixture->remote_node);
    ufo_daemon_stop (fixture->daemon, &error);
    g_assert_no_error (error);

    g_object_unref (fixture->daemon);
}

static void
test_remote_node_get_num_cpus (Fixture *fixture,
                               gconstpointer unused)
{
    guint n_gpus = ufo_remote_node_get_num_gpus (fixture->remote_node);
    g_debug ("Found %d number of GPUs at remotenode", n_gpus);
    g_assert (n_gpus > 0);
}

void
test_add_remote_node (const gchar* protocol)
{
    g_test_add ("/opencl/remotenode/get_num_cpus",
                Fixture, protocol,
                setup, test_remote_node_get_num_cpus, teardown);
}
