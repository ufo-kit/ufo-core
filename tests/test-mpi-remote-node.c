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

/* needed to avoid emty translation unit warning */
void __unused_test_mpi_remote_fn_(void);

#ifdef MPI

#include <string.h>
#include <ufo/ufo.h>
#include "test-suite.h"
#include <mpi.h>
#include <unistd.h>

typedef struct {
    UfoDaemon *daemon;
    UfoConfig *config;
    UfoRemoteNode **remote_nodes;
    gint global_size;
    gint rank;
} Fixture;

static void
setup (Fixture *fixture, gconstpointer data)
{
    fixture->config = ufo_config_new ();
    int size, rank;

    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);
    fixture->global_size = size;
    fixture->rank = rank;

    if (fixture->rank == 0) {
        g_message ("Number of mpi processes: %d", fixture->global_size);
        g_message ("Number of remote_nodes: %d", fixture->global_size - 1);
        fixture->remote_nodes = g_malloc0 (fixture->global_size - 1);
        // create remote nodes
        for (int i = 0; i < fixture->global_size - 1; i++) {
            gchar *addr = g_strdup_printf("%d", i+1);
            fixture->remote_nodes[i] = (UfoRemoteNode *) ufo_remote_node_new (addr);
        }
    } else {
        gchar *addr = g_strdup_printf("%d", rank);
        fixture->daemon = ufo_daemon_new (fixture->config, addr);
        ufo_daemon_start (fixture->daemon);
    }
}

static void
teardown (Fixture *fixture, gconstpointer data)
{
    g_message ("teardown");
    if (fixture->rank == 0) {
        for (int i = 1; i <= fixture->global_size - 1; i++) {
            UfoRemoteNode *node = fixture->remote_nodes[i-1];
            ufo_remote_node_terminate (node);
            g_object_unref (node);
            g_message ("teardown node %d done", i-1);
        }
    } else {
        ufo_daemon_wait_finish (fixture->daemon);
        g_object_unref (fixture->daemon);
        g_message ("teardown done");
    }
}

static void
test_remote_node_get_num_cpus (Fixture *fixture,
                               gconstpointer unused)
{
    if (fixture->rank == 0) {
        for (int i = 1; i <= fixture->global_size - 1; i++) {
            guint n_gpus = ufo_remote_node_get_num_gpus (fixture->remote_nodes[i-1]);
            g_message ("Found %d number of GPUs at remotenode %d", n_gpus, i);
            g_assert (n_gpus > 0);
        }
    }
}

static void
test_remote_node_get_structure (Fixture *fixture,
                               gconstpointer unused)
{
    if (fixture->rank == 0) {
        for (int i = 1; i <= fixture->global_size - 1 ; i++) {
            UfoTaskMode mode;
            UfoInputParam *in_params;
            guint n_inputs;
            ufo_remote_node_get_structure (fixture->remote_nodes[i-1], &n_inputs, &in_params, &mode);
            g_message ("received n_inputs == %d from remote node %d", n_inputs, i);
            g_assert (n_inputs == 1);
            g_message ("received n_dims == %d from remote node %d", in_params->n_dims, i);
            g_assert (in_params->n_dims == 2);
        }
    }
}

void
test_add_mpi_remote_node (void)
{
    g_test_add ("/remotenode/get_structure",
                Fixture, NULL,
                setup, test_remote_node_get_structure, teardown);
    g_test_add ("/remotenode/get_num_cpus",
                Fixture, NULL,
                setup, test_remote_node_get_num_cpus, teardown);
}

#endif
