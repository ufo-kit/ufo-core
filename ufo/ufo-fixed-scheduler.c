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
#include "config.h"

#ifdef WITH_PYTHON
#include <Python.h>
#endif

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#include <ufo/ufo-buffer.h>
#include <ufo/ufo-config.h>
#include <ufo/ufo-configurable.h>
#include <ufo/ufo-fixed-scheduler.h>
#include <ufo/ufo-resources.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-task-iface.h>
#include <ufo/ufo-two-way-queue.h>
#include "ufo-priv.h"
#include "compat.h"


G_DEFINE_TYPE_WITH_CODE (UfoFixedScheduler, ufo_fixed_scheduler, UFO_TYPE_BASE_SCHEDULER,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_FIXED_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FIXED_SCHEDULER, UfoFixedSchedulerPrivate))

struct  _UfoFixedSchedulerPrivate {
    UfoArchGraph *arch;
};

typedef struct {
    UfoTask *from;
    UfoTask *to;
    guint port;
    UfoTwoWayQueue *queue;
} Connection;

typedef struct {
    GList *connections;
    GList *tasks;
} ProcessData;

typedef struct {
    UfoGraph *graph;
    UfoTask *task;
    GList *connections;
    cl_context context;
} TaskData;

enum {
    PROP_0,
    N_PROPERTIES,
};

static UfoBuffer *POISON_PILL = (UfoBuffer *) 0x1;

/**
 * UfoFixedSchedulerError:
 * @UFO_FIXED_SCHEDULER_ERROR_SETUP: Could not start scheduler due to error
 */
GQuark
ufo_fixed_scheduler_error_quark (void)
{
    return g_quark_from_static_string ("ufo-scheduler-error-quark");
}

/**
 * ufo_fixed_scheduler_new:
 * @config: A #UfoConfig or %NULL
 *
 * Creates a new #UfoFixedScheduler.
 *
 * Return value: A new #UfoFixedScheduler
 */
UfoBaseScheduler *
ufo_fixed_scheduler_new (UfoConfig *config)
{
    return UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_FIXED_SCHEDULER, "config", config, NULL));
}

/**
 * ufo_fixed_scheduler_get_arch:
 * @sched: A #UfoFixedScheduler object
 *
 * Get a #UfoArchGraph object to get GPU nodes for manual assignment to tasks.
 * If it does not exist, it is created on the fly.
 *
 * Returns: (transfer none): An #UfoArchGraph object to retrieve GPU nodes.
 */
UfoArchGraph *
ufo_fixed_scheduler_get_arch (UfoFixedScheduler *sched)
{

    g_return_val_if_fail (UFO_IS_FIXED_SCHEDULER (sched), NULL);

    if (sched->priv->arch == NULL) {
        UfoResources *resources;
        resources = ufo_base_scheduler_get_resources (UFO_BASE_SCHEDULER (sched));
        sched->priv->arch = UFO_ARCH_GRAPH (ufo_arch_graph_new (resources, NULL));
    }

    return sched->priv->arch;
}

static gboolean
pop_input_data (UfoTwoWayQueue **in_queues, UfoBuffer **inputs, guint n_inputs)
{
    for (guint i = 0; i < n_inputs; i++) {
        inputs[i] = ufo_two_way_queue_consumer_pop (in_queues[i]);

        if (inputs[i] == POISON_PILL)
            return FALSE;
    }

    return TRUE;
}

static void
release_input_data (UfoTwoWayQueue **in_queues, UfoBuffer **inputs, guint n_inputs)
{
    for (guint i = 0; i < n_inputs; i++)
        ufo_two_way_queue_consumer_push (in_queues[i], inputs[i]);
}

static UfoBuffer *
pop_output_data (UfoTwoWayQueue *queue, UfoRequisition *requisition, cl_context context)
{
    if (ufo_two_way_queue_get_capacity (queue) < 2) {
        UfoBuffer *buffer;

        buffer = ufo_buffer_new (requisition, context);
        ufo_two_way_queue_insert (queue, buffer);
    }

    return ufo_two_way_queue_producer_pop (queue);
}

static GList *
get_output_queue_list (TaskData *data)
{
    GList *result = NULL;
    GList *it;

    g_list_for (data->connections, it) {
        Connection *connection = (Connection *) it->data;

        if (connection->from == data->task)
            result = g_list_append (result, connection->queue);
    }

    return result;
}

static UfoTwoWayQueue **
get_input_queues (TaskData *data, guint *n_inputs)
{
    UfoTwoWayQueue **result;
    GList *it;

    *n_inputs = ufo_graph_get_num_predecessors (data->graph, UFO_NODE (data->task));
    result = g_new0 (UfoTwoWayQueue *, *n_inputs);

    g_list_for (data->connections, it) {
        Connection *connection = (Connection *) it->data;

        if (connection->to == data->task)
            result[connection->port] = connection->queue;
    }

    return result;
}

static void
finish_successors (GList *out_queues)
{
    GList *it;

    g_list_for (out_queues, it) {
        UfoTwoWayQueue *out_queue = (UfoTwoWayQueue *) it->data;
        ufo_two_way_queue_producer_push (out_queue, POISON_PILL);
    }
}

static void
generate_loop (TaskData *data)
{
    UfoRequisition requisition;
    UfoBuffer *output;
    GList *out_queues;
    GList *it;
    gboolean active = TRUE;

    out_queues = get_output_queue_list (data);

    while (active) {
        ufo_task_get_requisition (data->task, NULL, &requisition);

        g_list_for (out_queues, it) {
            UfoTwoWayQueue *out_queue = (UfoTwoWayQueue *) it->data;

            output = pop_output_data (out_queue, &requisition, data->context);
            active = ufo_task_generate (data->task, output, &requisition);

            if (!active)
                break;

            ufo_two_way_queue_producer_push (out_queue, output);
        }
    }

    finish_successors (out_queues);
    g_list_free (out_queues);
}

static void
process_loop (TaskData *data)
{
    UfoRequisition requisition;
    UfoBuffer **inputs;
    UfoBuffer *output;
    UfoTwoWayQueue **in_queues;
    GList *out_queues;
    GList *it;
    guint n_inputs;
    gboolean active = TRUE;
    gboolean is_sink;

    in_queues = get_input_queues (data, &n_inputs);
    out_queues = get_output_queue_list (data);
    inputs = g_new0 (UfoBuffer *, n_inputs);
    is_sink = g_list_length (out_queues) == 0;

    while (active) {
        active = pop_input_data (in_queues, inputs, n_inputs);

        if (!active)
            break;

        ufo_task_get_requisition (data->task, inputs, &requisition);

        if (is_sink) {
            active = ufo_task_process (data->task, inputs, NULL, &requisition);
        }
        else {
            g_list_for (out_queues, it) {
                UfoTwoWayQueue *out_queue = (UfoTwoWayQueue *) it->data;

                output = pop_output_data (out_queue, &requisition, data->context);
                active = ufo_task_process (data->task, inputs, output, &requisition);

                if (!active)
                    break;

                ufo_two_way_queue_producer_push (out_queue, output);
            }
        }

        release_input_data (in_queues, inputs, n_inputs);
    }

    finish_successors (out_queues);

    g_free (in_queues);
    g_free (inputs);
    g_list_free (out_queues);
}

static void
reduce_loop (TaskData *data)
{
    UfoRequisition requisition;
    UfoTwoWayQueue **in_queues;
    UfoTwoWayQueue **output_queues;
    UfoBuffer **inputs;
    UfoBuffer **outputs;
    GList *it;
    GList *out_queues;
    guint n_inputs;
    guint n_outputs;
    gboolean active = TRUE;

    in_queues = get_input_queues (data, &n_inputs);
    out_queues = get_output_queue_list (data);
    inputs = g_new0 (UfoBuffer *, n_inputs);

    n_outputs = g_list_length (out_queues);
    outputs = g_new0 (UfoBuffer *, n_outputs);
    output_queues = g_new0 (UfoTwoWayQueue *, n_outputs);
    it = g_list_first (out_queues);

    for (guint i = 0; it != NULL; it = g_list_next (it)) {
        output_queues[i] = (UfoTwoWayQueue *) it->data;
    }

    /* Read first input item */
    if (!pop_input_data (in_queues, inputs, n_inputs))
        return;

    ufo_task_get_requisition (data->task, inputs, &requisition);

    /* Get the scratchpad output buffers from all successors */
    for (guint i = 0; i < n_outputs; i++) {
        outputs[i] = pop_output_data (output_queues[i], &requisition, data->context);
    }

    /* Process all inputs. Note that we already fetched the first input. */
    do {
        for (guint i = 0; i < n_outputs; i++) {
            active = ufo_task_process (data->task, inputs, outputs[i], &requisition);
            release_input_data (in_queues, inputs, n_inputs);
            active = pop_input_data (in_queues, inputs, n_inputs);
        }
    } while (active);

    /* Generate all outputs */
    do {
        for (guint i = 0; i < n_outputs; i++) {
            active = ufo_task_generate (data->task, outputs[i], &requisition);

            if (active) {
                ufo_two_way_queue_producer_push (output_queues[i], outputs[i]);
                outputs[i] = ufo_two_way_queue_producer_pop (output_queues[i]);
            }
        }
    } while (active);

    finish_successors (out_queues);

    g_free (inputs);
    g_free (in_queues);

    g_free (outputs);
    g_free (output_queues);
    g_list_free (out_queues);
}

static gpointer
run_local (TaskData *data)
{
    UfoTaskMode mode;

    mode = ufo_task_get_mode (data->task) & UFO_TASK_MODE_TYPE_MASK;

    switch (mode) {
        case UFO_TASK_MODE_GENERATOR:
            generate_loop (data);
            break;

        case UFO_TASK_MODE_PROCESSOR:
            process_loop (data);
            break;

        case UFO_TASK_MODE_REDUCTOR:
            reduce_loop (data);
            break;

        default:
            g_warning ("Unknown task mode");
    }

    /* We can release "data" here, because we do not store it when creating it */
    g_free (data);

    return NULL;
}

static void
join_threads (GList *threads)
{
    GList *it;

    g_list_for (threads, it) {
        g_thread_join (it->data);
    }
}

static GList *
append_if_not_existing (GList *list, UfoTask *task)
{
    if (g_list_find (list, task) == NULL)
        return g_list_append (list, task);

    return list;
}

static ProcessData *
setup_tasks (UfoGraph *graph,
             UfoArchGraph *arch,
             UfoResources *resources,
             GError **error)
{
    ProcessData *data;
    GList *gpu_nodes;
    GList *nodes;
    GList *it;

    data = g_new0 (ProcessData, 1);

    data->connections = NULL;
    data->tasks = NULL;

    nodes = ufo_graph_get_nodes (graph);
    gpu_nodes = ufo_arch_graph_get_gpu_nodes (arch);

    g_list_for (nodes, it) {
        UfoNode *source_node;
        UfoTask *source_task;
        GList *successors;
        GList *jt;

        source_node = UFO_NODE (it->data);
        source_task = UFO_TASK (source_node);
        data->tasks = append_if_not_existing (data->tasks, source_task);

        successors = ufo_graph_get_successors (graph, source_node);

        g_list_for (successors, jt) {
            UfoNode *dest_node;
            UfoTask *dest_task;
            Connection *connection;

            dest_node = UFO_NODE (jt->data);
            dest_task = UFO_TASK (dest_node);

            connection = g_new0 (Connection, 1);
            connection->from = source_task;
            connection->to = dest_task;
            connection->port = (guint) GPOINTER_TO_INT (ufo_graph_get_edge_label (graph, source_node, dest_node));
            connection->queue = ufo_two_way_queue_new (NULL);

            data->connections = g_list_append (data->connections, connection);
            data->tasks = append_if_not_existing (data->tasks, dest_task);
        }
    }

    g_list_for (data->tasks, it) {
        UfoTask *task;

        task = UFO_TASK (it->data);

        /* Set a default GPU if not assigned by user */
        if (ufo_task_get_mode (task) & UFO_TASK_MODE_GPU) {

            if (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)) == NULL) {
                if (g_list_length (gpu_nodes) == 0) {
                    g_set_error_literal (error, UFO_BASE_SCHEDULER_ERROR, UFO_BASE_SCHEDULER_ERROR_SETUP,
                                         "Using GPU tasks but no GPU available");
                    break;
                }

                g_debug ("Setting default GPU for %s-%p\n", ufo_task_node_get_plugin_name (UFO_TASK_NODE (task)), (gpointer) task);
                ufo_task_node_set_proc_node (UFO_TASK_NODE (task), g_list_nth_data (gpu_nodes, 0));
            }
        }

        ufo_task_setup (task, resources, error);

        if (*error != NULL)
            break;
    }

    g_list_free (nodes);

    return data;
}

static void
ufo_fixed_scheduler_run (UfoBaseScheduler *scheduler,
                         UfoTaskGraph *task_graph,
                         GError **error)
{
    UfoArchGraph *arch;
    UfoResources *resources;
    ProcessData *pdata;
    GList *threads;
    GList *it;
    GError *tmp_error = NULL;

    g_return_if_fail (UFO_IS_FIXED_SCHEDULER (scheduler));

    resources = ufo_base_scheduler_get_resources (scheduler);
    arch = ufo_fixed_scheduler_get_arch (UFO_FIXED_SCHEDULER (scheduler));
    pdata = setup_tasks (UFO_GRAPH (task_graph), arch, resources, &tmp_error);

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return;
    }

    threads = NULL;

    g_list_for (pdata->tasks, it) {
        GThread *thread;
        TaskData *tdata;

        tdata = g_new0 (TaskData, 1);
        tdata->graph = UFO_GRAPH (task_graph);
        tdata->task = UFO_TASK (it->data);
        tdata->connections = pdata->connections;
        tdata->context = ufo_resources_get_context (resources);
        thread = g_thread_create ((GThreadFunc) run_local, tdata, TRUE, error);
        threads = g_list_append (threads, thread);
    }

#ifdef WITH_PYTHON
    if (Py_IsInitialized ()) {
        Py_BEGIN_ALLOW_THREADS

        join_threads (threads);

        Py_END_ALLOW_THREADS
    }
    else {
        join_threads (threads);
    }
#else
    join_threads (threads);
#endif

    g_list_free (threads);
}

static void
ufo_fixed_scheduler_dispose (GObject *object)
{
    UfoFixedSchedulerPrivate *priv;

    priv = UFO_FIXED_SCHEDULER_GET_PRIVATE (object);

    if (priv->arch != NULL) {
        g_object_unref (priv->arch);
        priv->arch = NULL;
    }

    G_OBJECT_CLASS (ufo_fixed_scheduler_parent_class)->dispose (object);
}

static void
ufo_fixed_scheduler_class_init (UfoFixedSchedulerClass *klass)
{
    GObjectClass *oclass;
    UfoBaseSchedulerClass *sclass;

    sclass = UFO_BASE_SCHEDULER_CLASS (klass);
    sclass->run = ufo_fixed_scheduler_run;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_fixed_scheduler_dispose;

    g_type_class_add_private (klass, sizeof (UfoFixedSchedulerPrivate));
}

static void
ufo_fixed_scheduler_init (UfoFixedScheduler *scheduler)
{
    scheduler->priv = UFO_FIXED_SCHEDULER_GET_PRIVATE (scheduler);
    scheduler->priv->arch = NULL;
}
