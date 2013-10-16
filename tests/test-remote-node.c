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
#include <ufo.h>
#include "test-suite.h"
#include <mpi.h>
#include <unistd.h>

typedef struct {
    UfoDaemon *daemon;
    UfoConfig *config;
    UfoRemoteNode *remote_node;
    gint global_size;
    gint rank;
} Fixture;

static void
setup (Fixture *fixture, gconstpointer data)
{
    fixture->config = ufo_config_new ();

    // only call MPI_init once
    MPI_Comm_rank (MPI_COMM_WORLD, &fixture->rank);
    MPI_Comm_size (MPI_COMM_WORLD, &fixture->global_size);

    g_message("num of nodes: %d", fixture->global_size);

    // gchar *addr = g_strdup ("tcp://127.0.0.1:5555");
    if (fixture->rank == 0) {
        // connect to the other nodes
        // for (int i=1; i < fixture->global_size; i++) {
            gchar *addr = g_strdup_printf("%i", 1);
            fixture->remote_node = (UfoRemoteNode *) ufo_remote_node_new (addr);
        // }
    } else {
        // int j = 0;
        // printf("PID %d ready for attach\n", getpid());
        // while (0 == j)
        //     sleep(5);

        fixture->daemon = ufo_daemon_new (fixture->config, g_strdup ("1"));
        //         int j = 0;
        // printf("PID %d ready for attach\n", getpid());
        // while (0 == j)
        //     sleep(5);

        ufo_daemon_start (fixture->daemon);
        sleep (5);
    }
}

static void
teardown (Fixture *fixture, gconstpointer data)
{
    g_message("teardown");
    if (fixture->rank == 0) {
        g_object_unref (fixture->remote_node);
    } else {
        ufo_daemon_stop (fixture->daemon);
        g_object_unref (fixture->daemon);
    }
}

static void
test_remote_node_get_num_cpus (Fixture *fixture,
                               gconstpointer unused)
{
    if (fixture->rank == 0) {
        guint n_gpus = ufo_remote_node_get_num_gpus (fixture->remote_node);
        g_debug ("Found %d number of GPUs at remotenode", n_gpus);
        g_assert (n_gpus > 0);
    }
}

static void
test_remote_node_get_structure (Fixture *fixture,
                               gconstpointer unused)
{
    if (fixture->rank == 0) {
        UfoTaskMode mode;
        UfoInputParam *in_params;
        guint n_inputs;
        ufo_remote_node_get_structure (fixture->remote_node, &n_inputs, &in_params, &mode);
        g_message ("received n_inputs == %d", n_inputs);
        g_assert (n_inputs == 1);
        g_message ("received n_dims == %d", in_params->n_dims);
        g_assert (in_params->n_dims == 2);
    }
}

void
test_add_remote_node (void)
{
    MPI_Init (0, NULL);
    g_test_add ("/remotenode/get_structure",
                Fixture, NULL,
                setup, test_remote_node_get_structure, teardown);
    g_test_add ("/remotenode/get_num_cpus",
                Fixture, NULL,
                setup, test_remote_node_get_num_cpus, teardown);
}
