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
#include <ufo/ufo-fixed-scheduler.h>
#include <ufo/ufo-resources.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-task-iface.h>
#include <ufo/ufo-two-way-queue.h>
#include "ufo-priv.h"
#include "compat.h"

/**
 * SECTION:ufo-fixed-scheduler
 * @Short_description: Simple fixed scheduler
 * @Title: UfoFixedScheduler
 *
 * This scheduler has only minimal automatisms. It does not attempt to
 * distribute work among multiple GPUs, which is left to do by the user.
 */

G_DEFINE_TYPE (UfoFixedScheduler, ufo_fixed_scheduler, UFO_TYPE_BASE_SCHEDULER)

#define UFO_FIXED_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FIXED_SCHEDULER, UfoFixedSchedulerPrivate))

typedef struct {
    UfoTask *from;
    UfoTask *to;
    guint port;
    UfoTwoWayQueue *queue;
} Connection;

typedef struct {
    GList *connections;
    GList *tasks;
    GList *queues;
} ProcessData;

typedef struct {
    UfoGraph *graph;
    UfoTask *task;
    GList *connections;
    UfoTwoWayQueue **in_queues;
    UfoTwoWayQueue *out_queue;
    cl_context context;
} TaskData;

struct _UfoFixedSchedulerPrivate {
    GHashTable *task_data;
};

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
 *
 * Creates a new #UfoFixedScheduler.
 *
 * Return value: A new #UfoFixedScheduler
 */
UfoBaseScheduler *
ufo_fixed_scheduler_new (void)
{
    return UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_FIXED_SCHEDULER, NULL));
}

static gboolean
pop_input_data (UfoTwoWayQueue **in_queues, gboolean *finished, UfoBuffer **inputs, guint n_inputs)
{
    guint n_finished;

    n_finished = 0;

    for (guint i = 0; i < n_inputs; i++) {
        if (!finished[i]) {
            UfoBuffer *input;

            input = ufo_two_way_queue_consumer_pop (in_queues[i]);

            if (input == POISON_PILL) {
                finished[i] = TRUE;
                n_finished++;
            }
            else {
                inputs[i] = input;
            }
        }
        else {
            n_finished++;
        }
    }

    return n_finished < n_inputs;
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
    UfoBuffer *buffer;

    if (ufo_two_way_queue_get_capacity (queue) < 2) {
        buffer = ufo_buffer_new (requisition, context);
        ufo_two_way_queue_insert (queue, buffer);
    }

    buffer = ufo_two_way_queue_producer_pop (queue);

    if (ufo_buffer_cmp_dimensions (buffer, requisition))
        ufo_buffer_resize (buffer, requisition);

    return buffer;
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
    /* GList *out_queues; */
    /* GList *it; */
    gboolean active = TRUE;

    /* out_queues = get_output_queue_list (data); */

    while (active) {
        /* g_list_for (out_queues, it) { */
            /* UfoTwoWayQueue *out_queue = (UfoTwoWayQueue *) it->data; */

            ufo_task_get_requisition (data->task, NULL, &requisition);
            output = pop_output_data (data->out_queue, &requisition, data->context);
            active = ufo_task_generate (data->task, output, &requisition);

            if (!active)
                break;

            ufo_two_way_queue_producer_push (data->out_queue, output);
        /* } */
    }

    /* finish_successors (out_queues); */
    ufo_two_way_queue_producer_push (data->out_queue, POISON_PILL);
    ufo_two_way_queue_producer_push (data->out_queue, POISON_PILL);
    g_print ("%s: done\n", G_OBJECT_TYPE_NAME (data->task));
    /* g_list_free (out_queues); */
}

static void
process_loop (TaskData *data)
{
    UfoRequisition requisition;
    UfoBuffer **inputs;
    UfoBuffer *output;
    /* UfoTwoWayQueue **in_queues; */
    gboolean *finished;
    /* GList *out_queues; */
    GList *it;
    guint n_inputs;
    gboolean active = TRUE;
    gboolean is_sink;

    /* in_queues = get_input_queues (data, &n_inputs); */
    /* out_queues = get_output_queue_list (data); */
    n_inputs = ufo_task_get_num_inputs (data->task);
    inputs = g_new0 (UfoBuffer *, n_inputs);
    finished = g_new0 (gboolean, n_inputs);
    is_sink = ufo_task_get_mode (data->task) & UFO_TASK_MODE_SINK; //  g_list_length (out_queues) == 0;

    while (active) {
        g_print ("%s-%p: reading data from %p\n", G_OBJECT_TYPE_NAME (data->task), data->task, data->in_queues[0]);
        active = pop_input_data (data->in_queues, finished, inputs, n_inputs);

        if (!active)
            break;

        ufo_task_get_requisition (data->task, inputs, &requisition);

        if (is_sink) {
            active = ufo_task_process (data->task, inputs, NULL, &requisition);
        }
        else {
            /* g_list_for (out_queues, it) { */
            /*     UfoTwoWayQueue *out_queue = (UfoTwoWayQueue *) it->data; */

                output = pop_output_data (data->out_queue, &requisition, data->context);

                for (guint i = 0; i < n_inputs; i++)
                    ufo_buffer_copy_metadata (inputs[i], output);

                active = ufo_task_process (data->task, inputs, output, &requisition);

                if (!active)
                    break;

                g_print ("%s-%p: pushed data to %p\n", G_OBJECT_TYPE_NAME (data->task), data->task, data->out_queue);
                ufo_two_way_queue_producer_push (data->out_queue, output);
            /* } */
        }

        release_input_data (data->in_queues, inputs, n_inputs);
    }

    if (!is_sink) {
    /* finish_successors (out_queues); */
    /* ufo_two_way_queue_producer_push (data->out_queue, POISON_PILL); */
    g_print ("%s: pushed pill to %p\n", G_OBJECT_TYPE_NAME (data->task), data->out_queue);
    }

    /* g_free (in_queues); */
    g_free (inputs);
    g_free (finished);
    /* g_list_free (out_queues); */
}

static void
reduce_loop (TaskData *data)
{
#if 0
    UfoRequisition requisition;
    /* UfoTwoWayQueue **in_queues; */
    /* UfoTwoWayQueue **output_queues; */
    UfoBuffer **inputs;
    UfoBuffer **outputs;
    gboolean *finished;
    GList *it;
    GList *out_queues;
    guint n_inputs;
    guint n_outputs;
    gboolean active = TRUE;

    /* in_queues = get_input_queues (data, &n_inputs); */
    /* out_queues = get_output_queue_list (data); */
    inputs = g_new0 (UfoBuffer *, n_inputs);
    finished = g_new0 (gboolean, n_inputs);

    /* n_outputs = g_list_length (out_queues); */
    /* outputs = g_new0 (UfoBuffer *, n_outputs); */
    /* output_queues = g_new0 (UfoTwoWayQueue *, n_outputs); */
    /* it = g_list_first (out_queues); */

    for (guint i = 0; it != NULL; it = g_list_next (it)) {
        output_queues[i] = (UfoTwoWayQueue *) it->data;
    }

    /* Read first input item */
    if (!pop_input_data (in_queues, finished, inputs, n_inputs))
        return;

    ufo_task_get_requisition (data->task, inputs, &requisition);

    /* Get the scratchpad output buffers from all successors */
    for (guint i = 0; i < n_outputs; i++) {
        outputs[i] = pop_output_data (output_queues[i], &requisition, data->context);
    }

    do {
        gboolean go_on = TRUE;

        /* Process all inputs. Note that we already fetched the first input. */
        do {
            for (guint i = 0; i < n_outputs; i++) {
                for (guint j = 0; j < n_inputs; j++)
                    ufo_buffer_copy_metadata (inputs[j], outputs[i]);

                go_on = ufo_task_process (data->task, inputs, outputs[i], &requisition);
                release_input_data (in_queues, inputs, n_inputs);
                active = pop_input_data (in_queues, finished, inputs, n_inputs);
                go_on = go_on && active;
            }
        } while (go_on);

        /* Generate all outputs */
        do {
            for (guint i = 0; i < n_outputs; i++) {
                go_on = ufo_task_generate (data->task, outputs[i], &requisition);

                if (go_on) {
                    ufo_two_way_queue_producer_push (output_queues[i], outputs[i]);
                    outputs[i] = ufo_two_way_queue_producer_pop (output_queues[i]);
                }
            }
        } while (go_on);
    } while (active);

    finish_successors (out_queues);

    g_free (inputs);
    g_free (in_queues);

    g_free (outputs);
    g_free (output_queues);
    g_free (finished);
    g_list_free (out_queues);
#endif
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
        case UFO_TASK_MODE_SINK:
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

#if 0
static GList *
append_if_not_existing (GList *list, UfoTask *task)
{
    if (g_list_find (list, task) == NULL)
        return g_list_append (list, task);

    return list;
}

static ProcessData *
setup_tasks (UfoGraph *graph,
             UfoBaseScheduler *scheduler,
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
    data->queues = NULL;

    nodes = ufo_graph_get_nodes (graph);
    gpu_nodes = ufo_resources_get_gpu_nodes (resources);

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

            data->queues = g_list_append (data->queues, connection->queue);
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

                g_debug ("Setting default GPU %p for %s-%p",
                         g_list_nth_data (gpu_nodes, 0),
                         ufo_task_node_get_plugin_name (UFO_TASK_NODE (task)), (gpointer) task);

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
#endif

static void
create_task_data (UfoGraph *graph, UfoTask *task, GHashTable *task_to_data)
{
    GList *successors;
    GList *it;

    successors = ufo_graph_get_successors (graph, dest);

    g_list_for (successors, it)
        create_task_data (graph, UFO_TASK (it->data), task_to_data);

    g_list_free (successors);
}

static void
ufo_fixed_scheduler_run (UfoBaseScheduler *scheduler,
                         UfoTaskGraph *task_graph,
                         GError **error)
{
    UfoResources *resources;
    ProcessData *pdata;
    GList *threads;
    GList *nodes;
    GList *gpu_nodes; /* FIXME: free */
    GList *it;
    GError *tmp_error = NULL;
    GHashTable *task_to_data;

    g_return_if_fail (UFO_IS_FIXED_SCHEDULER (scheduler));

    resources = ufo_base_scheduler_get_resources (scheduler);
    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    gpu_nodes = ufo_resources_get_gpu_nodes (resources);
    task_to_data = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

#if 0
    /* Create in-queues */
    g_list_for (nodes, it) {
        UfoTask *task;
        TaskData *data;
        guint n_inputs;

        data = g_new0 (TaskData, 1);
        task = UFO_TASK (it->data);
        n_inputs = ufo_task_get_num_inputs (task);

        data->task = task;
        data->context = ufo_resources_get_context (resources);
        data->in_queues = g_new0 (UfoTwoWayQueue *, n_inputs);

        for (guint i = 0; i < n_inputs; i++)
            data->in_queues[i] = ufo_two_way_queue_new (NULL);

        if (ufo_task_get_mode (task) & UFO_TASK_MODE_GPU) {
            if (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)) == NULL) {
                if (g_list_length (gpu_nodes) == 0) {
                    g_set_error_literal (error, UFO_BASE_SCHEDULER_ERROR, UFO_BASE_SCHEDULER_ERROR_SETUP,
                                         "Using GPU tasks but no GPU available");
                    break;
                }

                g_debug ("Setting default GPU %p for %s-%p",
                         g_list_nth_data (gpu_nodes, 0),
                         ufo_task_node_get_plugin_name (UFO_TASK_NODE (task)), (gpointer) task);

                ufo_task_node_set_proc_node (UFO_TASK_NODE (task), g_list_nth_data (gpu_nodes, 0));
            }
        }

        ufo_task_setup (task, resources, &tmp_error);
        g_hash_table_insert (task_to_data, task, data);
    }

    /* Hook up out-queues */
    g_list_for (nodes, it) {
        UfoNode *dest;
        TaskData *dest_data;
        GList *predecessors;
        GList *jt;

        dest = UFO_NODE (it->data);
        dest_data = g_hash_table_lookup (task_to_data, dest);
        predecessors = ufo_graph_get_predecessors (UFO_GRAPH (task_graph), dest);

        g_list_for (predecessors, jt) {
            UfoNode *source;
            TaskData *source_data;
            guint port;
            
            source = UFO_NODE (jt->data);
            source_data = g_hash_table_lookup (task_to_data, source);
            g_assert (source_data != NULL);
            port = (guint) GPOINTER_TO_INT (ufo_graph_get_edge_label (UFO_GRAPH (task_graph), source, dest));

            g_print ("%s-%p -> [%i] -> %s-%p\n", G_OBJECT_TYPE_NAME (source), source, port, G_OBJECT_TYPE_NAME (dest), dest);
            g_assert (dest_data->in_queues[port] != NULL);
            source_data->out_queue = dest_data->in_queues[port];
        }

        g_list_free (predecessors);
    }

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return;
    }

    threads = NULL;

    g_list_for (nodes, it) {
        GThread *thread;
        TaskData *data;

        data = g_hash_table_lookup (task_to_data, it->data);
        g_print ("starting %s-%p\n", G_OBJECT_TYPE_NAME (data->task), data->task);
        thread = g_thread_create ((GThreadFunc) run_local, data, TRUE, error);
        threads = g_list_append (threads, thread);
    }
#endif

#if 0
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
#endif

#if 0
#ifdef WITH_PYTHON
    if (Py_IsInitialized ()) {
        PyGILState_STATE state = PyGILState_Ensure ();
        Py_BEGIN_ALLOW_THREADS

        join_threads (threads);

        Py_END_ALLOW_THREADS
        PyGILState_Release (state);
    }
    else {
        join_threads (threads);
    }
#else
    join_threads (threads);
#endif

    g_list_for (pdata->queues, it) {
        UfoTwoWayQueue *queue;
        GList *buffers;
        GList *jt;

        queue = (UfoTwoWayQueue *) it->data;
        buffers = ufo_two_way_queue_get_inserted (queue);

        g_list_for (buffers, jt)
            g_object_unref (jt->data);

        ufo_two_way_queue_free (queue);
    }
#endif

    g_list_free (threads);
}

static void
ufo_fixed_scheduler_class_init (UfoFixedSchedulerClass *klass)
{
    UfoBaseSchedulerClass *sclass;

    sclass = UFO_BASE_SCHEDULER_CLASS (klass);
    sclass->run = ufo_fixed_scheduler_run;

    g_type_class_add_private (klass, sizeof (UfoFixedSchedulerPrivate));
}

static void
ufo_fixed_scheduler_init (UfoFixedScheduler *scheduler)
{
    scheduler->priv = UFO_FIXED_SCHEDULER_GET_PRIVATE (scheduler);
}
