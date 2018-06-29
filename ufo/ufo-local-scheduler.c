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

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#ifdef WITH_PYTHON
#include <Python.h>
#endif

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-buffer.h"
#include "ufo-resources.h"
#include "ufo-local-scheduler.h"
#include "ufo-task-node.h"
#include "ufo-task-iface.h"
#include "ufo-two-way-queue.h"
#include "ufo-priv.h"

/**
 * SECTION:ufo-local-scheduler
 * @Short_description: Schedule each task independently
 * @Title: UfoLocalScheduler
 *
 * This scheduler schedules each task autonomously without taking relations
 * between tasks into account. It is not recommended to use this scheduler in
 * production.
 */

G_DEFINE_TYPE (UfoLocalScheduler, ufo_local_scheduler, UFO_TYPE_BASE_SCHEDULER)

#define UFO_LOCAL_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_LOCAL_SCHEDULER, UfoLocalSchedulerPrivate))


typedef struct {
    GQueue *queue;
    GMutex lock;
} ProcessorPool;

typedef struct {
    gpointer context;
    ProcessorPool *pp;
    UfoTask *task;
    UfoTwoWayQueue **inputs;
    UfoTwoWayQueue *output;
    guint n_inputs;
    gboolean is_leaf;
} TaskLocal;

enum {
    PROP_0,
    N_PROPERTIES,
};

static UfoBuffer *POISON_PILL = (UfoBuffer *) 0x1;

/**
 * UfoLocalSchedulerError:
 * @UFO_LOCAL_SCHEDULER_ERROR_SETUP: Could not start scheduler due to error
 */
GQuark
ufo_local_scheduler_error_quark (void)
{
    return g_quark_from_static_string ("ufo-scheduler-error-quark");
}

static ProcessorPool *
ufo_pp_new (GList *init)
{
    ProcessorPool *pp;
    GList *jt;

    pp = g_malloc0 (sizeof (ProcessorPool));
    pp->queue = g_queue_new ();
    g_mutex_init (&pp->lock);

    g_list_for (init, jt) {
        g_queue_push_tail (pp->queue, jt->data);
    }

    return pp;
}

static void
ufo_pp_destroy (ProcessorPool *pp)
{
    g_queue_free (pp->queue);
    g_mutex_clear (&pp->lock);
}

static gpointer
ufo_pp_next (ProcessorPool *pp)
{
    gpointer data;

    g_mutex_lock (&pp->lock);
    data = g_queue_pop_head (pp->queue);
    g_queue_push_tail (pp->queue, data);
    g_mutex_unlock (&pp->lock);
    return data;
}

/**
 * ufo_local_scheduler_new:
 *
 * Creates a new #UfoLocalScheduler.
 *
 * Return value: A new #UfoLocalScheduler
 */
UfoBaseScheduler *
ufo_local_scheduler_new (void)
{
    return UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_LOCAL_SCHEDULER, NULL));
}

static gboolean
pop_input_data (TaskLocal *local, UfoBuffer **inputs)
{
    for (guint i = 0; i < local->n_inputs; i++) {
        inputs[i] = ufo_two_way_queue_consumer_pop (local->inputs[i]);

        if (inputs[i] == POISON_PILL)
            return FALSE;
    }

    return TRUE;
}

static void
release_input_data (TaskLocal *local, UfoBuffer **inputs)
{
    for (guint i = 0; i < local->n_inputs; i++)
        ufo_two_way_queue_consumer_push (local->inputs[i], inputs[i]);
}

static GError *
run_local (TaskLocal *local)
{
    UfoBuffer **inputs;
    UfoBuffer *output;
    UfoRequisition requisition;
    UfoTask *task;
    UfoTaskMode mode;
    UfoTaskMode pu_mode;
    GError *error = NULL;
    gboolean active = TRUE;

    task = local->task;
    inputs = g_new0 (UfoBuffer *, local->n_inputs);
    mode = ufo_task_get_mode (task) & UFO_TASK_MODE_TYPE_MASK;
    pu_mode = ufo_task_get_mode (task) & UFO_TASK_MODE_PROCESSOR_MASK;
    output = NULL;
    requisition.n_dims = 0;

    while (active) {
        /* Fetch data from parent locals */
        active = pop_input_data (local, inputs);

        if (!active) {
            ufo_task_inputs_stopped_callback (task);
            break;
        }

        /* Choose next task of the local */
        /* profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task)); */

        /* Ask current task about size requirements */
        ufo_task_get_requisition (task, inputs, &requisition, &error);

        /* Insert output buffers as longs as capacity is not filled */
        if (!local->is_leaf) {
            if (ufo_two_way_queue_get_capacity (local->output) < 2) {
                UfoBuffer *buffer;

                buffer = ufo_buffer_new (&requisition, local->context);
                ufo_two_way_queue_insert (local->output, buffer);
            }

            output = ufo_two_way_queue_producer_pop (local->output);
        }

        if (pu_mode == UFO_TASK_MODE_GPU) {
            ufo_task_node_set_proc_node (UFO_TASK_NODE (task),
                                         UFO_NODE (ufo_pp_next (local->pp)));
        }

        /* Generate/process the data. Because the functions return active state,
         * we negate it for the finished flag. */
        if (mode != UFO_TASK_MODE_REDUCTOR) {
            if (mode == UFO_TASK_MODE_PROCESSOR)
                active = ufo_task_process (task, inputs, output, &requisition);
            else
                active = ufo_task_generate (task, output, &requisition);

            if (output != NULL && active) {
                ufo_two_way_queue_producer_push (local->output, output);
            }

            release_input_data (local, inputs);
        }
        else {
            /* This branch handles the reductor mode */
            do {
                active = ufo_task_process (task, inputs, output, &requisition);
                release_input_data (local, inputs);
                active = pop_input_data (local, inputs);
                if (!active) {
                    ufo_task_inputs_stopped_callback (task);
                }
            } while (active);

            /* Generate and forward as long as reductor produces data */
            do {
                active = ufo_task_generate (task, output, &requisition);

                if (active) {
                    ufo_two_way_queue_producer_push (local->output, output);
                    output = ufo_two_way_queue_producer_pop (local->output);
                }
            } while (active);
        }
    }

    if (!local->is_leaf)
        ufo_two_way_queue_producer_push (local->output, POISON_PILL);

    return error;
}

static void
join_threads (GList *threads)
{
    GList *it;

    g_list_for (threads, it) {
        g_thread_join (it->data);
    }
}

static GHashTable *
setup_tasks (UfoGraph *graph,
             UfoResources *resources,
             ProcessorPool *pp,
             GError **error)
{
    GHashTable *local;
    GList *nodes;
    GList *it;

    local = g_hash_table_new (g_direct_hash, g_direct_equal);
    nodes = ufo_graph_get_nodes (graph);

    g_list_for (nodes, it) {
        UfoNode *node;
        UfoTask *task;
        TaskLocal *data;
        GList *successors;
        GList *predecessors;

        data = g_malloc0 (sizeof (TaskLocal));
        node = UFO_NODE (it->data);
        task = UFO_TASK (node);

        data->inputs = NULL;
        data->output = NULL;
        data->task = task;
        data->is_leaf = TRUE;
        data->pp = pp;
        data->n_inputs = ufo_task_get_num_inputs (task);
        data->context = ufo_resources_get_context (resources);

        g_hash_table_insert (local, node, data);
        successors = ufo_graph_get_successors (graph, UFO_NODE (task));
        predecessors = ufo_graph_get_predecessors (graph, UFO_NODE (task));

        if (g_list_length (successors) > 0) {
            UfoNode *succ;
            TaskLocal *succ_data;
            gint port;

            data->is_leaf = FALSE;
            succ = UFO_NODE (g_list_nth_data (successors, 0));
            succ_data = g_hash_table_lookup (local, succ);

            port = GPOINTER_TO_INT (ufo_graph_get_edge_label (graph, node, succ));

            if (succ_data != NULL) {
                data->output = succ_data->inputs[port];
            }
            else {
                data->output = ufo_two_way_queue_new (NULL);
            }
        }

        if (g_list_length (predecessors) > 0) {
            GList *jt;

            data->inputs = g_new0 (UfoTwoWayQueue *, data->n_inputs);

            g_list_for (predecessors, jt) {
                UfoNode *pred;
                TaskLocal *pred_data;
                gint port;

                pred = UFO_NODE (jt->data);
                pred_data = g_hash_table_lookup (local, pred);
                port = GPOINTER_TO_INT (ufo_graph_get_edge_label (graph, pred, node));

                if (pred_data != NULL) {
                    data->inputs[port] = pred_data->output;
                }
                else {
                    data->inputs[port] = ufo_two_way_queue_new (NULL);
                }
            }
        }

        ufo_task_setup (task, resources, error);

        g_list_free (successors);
        g_list_free (predecessors);
    }

    g_list_free (nodes);
    return local;
}

static void
ufo_local_scheduler_run (UfoBaseScheduler *scheduler,
                         UfoTaskGraph *task_graph,
                         GError **error)
{
    UfoResources *resources;
    ProcessorPool *pp;
    GHashTable *task_data;
    GList *local_data;
    GList *threads;
    GList *it;
    GList *gpu_nodes;

    g_return_if_fail (UFO_IS_LOCAL_SCHEDULER (scheduler));

    resources = ufo_base_scheduler_get_resources (scheduler, error);

    if (resources == NULL)
        return;

    gpu_nodes = ufo_resources_get_gpu_nodes (resources);
    pp = ufo_pp_new (gpu_nodes);
    g_list_free (gpu_nodes);

    task_data = setup_tasks (UFO_GRAPH (task_graph), resources, pp, error);
    local_data = g_hash_table_get_values (task_data);

    threads = NULL;

    g_list_for (local_data, it) {
        GThread *thread;

        thread = g_thread_new (NULL, (GThreadFunc) run_local, it->data);
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

    ufo_pp_destroy (pp);
    g_list_free (threads);
    g_hash_table_destroy (task_data);
}

static void
ufo_local_scheduler_class_init (UfoLocalSchedulerClass *klass)
{
    UfoBaseSchedulerClass *sclass;

    sclass = UFO_BASE_SCHEDULER_CLASS (klass);
    sclass->run = ufo_local_scheduler_run;
}

static void
ufo_local_scheduler_init (UfoLocalScheduler *scheduler)
{
}
