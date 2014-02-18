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
#include <stdio.h>
#include "test-suite.h"

typedef struct {
    UfoDaemon *daemon;
    UfoConfig *config;
    UfoRemoteNode *remote_node;
} Fixture;

static void
setup (Fixture *fixture, gconstpointer data)
{
    gchar *addr = g_strdup ("tcp://127.0.0.1:5555");
    fixture->config = ufo_config_new ();

    fixture->daemon = ufo_daemon_new (fixture->config, addr);
    ufo_daemon_start (fixture->daemon);

    fixture->remote_node = (UfoRemoteNode *) ufo_remote_node_new (addr);
}

static void
teardown (Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->remote_node);
    ufo_daemon_stop (fixture->daemon);

    g_object_unref (fixture->daemon);
}

static void
test_remote_node_get_num_gpus (Fixture *fixture,
                               gconstpointer unused)
{
    guint n_gpus = ufo_remote_node_get_num_gpus (fixture->remote_node);
    g_debug ("Found %d number of GPUs at remotenode", n_gpus);
    g_assert (n_gpus > 0);
}

static void
test_remote_node_get_num_cpus (Fixture *fixture,
                               gconstpointer unused)
{
    guint n_cpus = ufo_remote_node_get_num_cpus (fixture->remote_node);
    g_debug ("Found %d number of CPUs at remotenode", n_cpus);
    g_assert (n_cpus > 0);
}

static void
test_remote_node_get_structure (Fixture *fixture,
                               gconstpointer unused)
{
    UfoTaskMode mode;
    UfoInputParam *in_params;
    guint n_inputs;
    ufo_remote_node_get_structure (fixture->remote_node, &n_inputs, &in_params, &mode);
    g_message ("received n_inputs == %d", n_inputs);
    g_assert (n_inputs == 1);
    g_message ("received n_dims == %d", in_params->n_dims);
    g_assert (in_params->n_dims == 2);
}

static gchar *
read_file (const gchar *filename)
{
    FILE *fp = fopen (filename, "r");

    if (fp == NULL)
        return NULL;

    fseek (fp, 0, SEEK_END);
    const gsize length = (gsize) ftell (fp);
    rewind (fp);
    gchar *buffer = (gchar *) g_malloc0(length + 1);

    if (buffer == NULL) {
        fclose (fp);
        return NULL;
    }

    size_t buffer_length = fread (buffer, 1, length, fp);
    fclose (fp);

    if (buffer_length != length) {
        g_free (buffer);
        return NULL;
    }

    return buffer;
}

gboolean should_abort (const gchar *log_domain,
                       GLogLevelFlags log_level,
                       const gchar *message,
                       gpointer user_data)
{
    return FALSE;
}

static void
test_remote_node_send_inputs (Fixture *fixture,
                              gconstpointer unused)
{
    UfoRemoteNode *remote_node = fixture->remote_node;
    g_test_log_set_fatal_handler ((GTestLogFatalFunc) should_abort, NULL);

    gchar *json = read_file("test.json");
    ufo_remote_node_send_json(remote_node, UFO_REMOTE_MODE_STREAM, json);

    UfoRequisition *sample_req = g_malloc (sizeof(UfoRequisition));
    sample_req->n_dims = 2;
    sample_req->dims[0]= 799;
    sample_req->dims[1]= 799;

    UfoResources *res = ufo_resources_new(NULL, NULL);
    gpointer context = ufo_resources_get_context (res);
    UfoBuffer *buffer = ufo_buffer_new (sample_req, NULL, context);

    UfoBuffer **inputs = g_malloc(sizeof (UfoBuffer *));
    inputs[0] = buffer;

    UfoInputParam *in_params;
    UfoTaskMode *mode = UFO_TASK_MODE_PROCESSOR;
    guint n_inputs;
    ufo_remote_node_get_structure (remote_node, &n_inputs, &in_params, &mode);

    UfoRequisition req;
    UfoBuffer *output = NULL;

    GTimer *timer = g_timer_new ();

    gint num_inputs = 10;
    gint num_runs = 100;
    gfloat total = 0.0;

    for (guint i=0; i < num_runs; i++) {
        g_timer_reset (timer);
        for (gint j = 0; j < num_inputs; j++)
            ufo_remote_node_send_inputs (remote_node, inputs);

            ufo_remote_node_get_requisition(remote_node, &req);

        if (G_UNLIKELY(output == NULL))
            output = ufo_buffer_new (&req, NULL, context);

        for (gint j = 0; j < num_inputs; j++)
            ufo_remote_node_get_result (remote_node, output);

        gfloat iter = g_timer_elapsed (timer, NULL);

        if (i > 0)
            total += iter;
    }
    gfloat avg = total / (gfloat) (num_runs - 1);
    gfloat per_frame = avg / ((gfloat) num_inputs);
    g_message ("Iteration avg: %.4fs, per frame: %.6fs", avg, per_frame);
}

void
test_add_remote_node (void)
{
    g_test_add ("/remotenode/send_inputs",
                Fixture, NULL,
                setup, test_remote_node_send_inputs, teardown);
    g_test_add ("/remotenode/get_structure",
                Fixture, NULL,
                setup, test_remote_node_get_structure, teardown);
    g_test_add ("/remotenode/get_num_gpus",
                Fixture, NULL,
                setup, test_remote_node_get_num_gpus, teardown);
    g_test_add ("/remotenode/get_num_cpus",
                Fixture, NULL,
                setup, test_remote_node_get_num_cpus, teardown);
}
