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
#include "ufo-group-scheduler.h"
#include "ufo-task-node.h"
#include "ufo-task-iface.h"
#include "ufo-two-way-queue.h"
#include "ufo-priv.h"

/**
 * SECTION:ufo-group-scheduler
 * @Short_description: Schedule according to a grouping policy
 * @Title: UfoGroupScheduler
 *
 * Unlike the #UfoLocalScheduler, the #UfoGroupScheduler groups the same node
 * types together and assigns resources in a user-defined fashion. It is not
 * recommended to use this scheduler in production.
 */

G_DEFINE_TYPE (UfoGroupScheduler, ufo_group_scheduler, UFO_TYPE_BASE_SCHEDULER)

#define UFO_GROUP_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GROUP_SCHEDULER, UfoGroupSchedulerPrivate))


typedef struct {
    GList *parents;
    GList *tasks;
    gboolean is_leaf;
    gpointer context;
    UfoTwoWayQueue *queue;
    enum {
        TASK_GROUP_ROUND_ROBIN,
        TASK_GROUP_SHARED,
        TASK_GROUP_RANDOM
    } mode;
} TaskGroup;


enum {
    PROP_0,
    N_PROPERTIES,
};

static UfoBuffer *POISON_PILL = (UfoBuffer *) 0x1;

/**
 * UfoGroupSchedulerError:
 * @UFO_GROUP_SCHEDULER_ERROR_SETUP: Could not start scheduler due to error
 */
GQuark
ufo_group_scheduler_error_quark (void)
{
    return g_quark_from_static_string ("ufo-scheduler-error-quark");
}


/**
 * ufo_group_scheduler_new:
 *
 * Creates a new #UfoGroupScheduler.
 *
 * Return value: A new #UfoGroupScheduler
 */
UfoBaseScheduler *
ufo_group_scheduler_new (void)
{
    return UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_GROUP_SCHEDULER, NULL));
}

static gboolean
expand_group_graph (UfoResources *resources, UfoGraph *graph, GError **error)
{
    GList *nodes;
    GList *it;
    GList *gpu_nodes;
    guint n_gpus;
    gboolean success = TRUE;

    gpu_nodes = ufo_resources_get_gpu_nodes (resources);
    n_gpus = g_list_length (gpu_nodes);

    nodes = ufo_graph_get_nodes (graph);

    g_list_for (nodes, it) {
        TaskGroup *group;
        UfoTaskNode *node;

        group = ufo_node_get_label (UFO_NODE (it->data));
        node = UFO_TASK_NODE (group->tasks->data);

        if (ufo_task_uses_gpu (UFO_TASK (node))) {
            ufo_task_node_set_proc_node (node, g_list_first (gpu_nodes)->data);

            for (guint i = 1; i < n_gpus; i++) {
                UfoTaskNode *copy;

                copy = UFO_TASK_NODE (ufo_node_copy (UFO_NODE (node), error));

                if (copy == NULL) {
                    success = FALSE;
                    goto cleanup;
                }

                ufo_task_node_set_proc_node (copy, g_list_nth_data (gpu_nodes, i));
                group->tasks = g_list_append (group->tasks, copy);
            }
        }
    }

cleanup:
    g_list_free (gpu_nodes);
    g_list_free (nodes);
    return success;
}

static GHashTable *
build_task_groups (UfoBaseScheduler *scheduler, UfoTaskGraph *graph, UfoResources *resources, GList *nodes)
{
    GHashTable *tasks_to_groups;
    GList *it;

    tasks_to_groups = g_hash_table_new (g_direct_hash, g_direct_equal);

    /* Create a group with a single member for each node */
    g_list_for (nodes, it) {
        TaskGroup *group;
        UfoNode *task;
        UfoNode *node;

        task = UFO_NODE (it->data);
        group = g_new0 (TaskGroup, 1);
        group->context = ufo_resources_get_context (resources);
        group->parents = NULL;
        group->tasks = g_list_append (NULL, it->data);
        group->queue = ufo_two_way_queue_new (NULL);
        group->is_leaf = ufo_graph_get_num_successors (UFO_GRAPH (graph), task) == 0;

        if (ufo_task_get_mode (UFO_TASK (task)) & UFO_TASK_MODE_SHARE_DATA) {
            group->mode = TASK_GROUP_SHARED;
        }
        else {
            group->mode = TASK_GROUP_ROUND_ROBIN;
        }

        node = ufo_node_new (group);
        g_hash_table_insert (tasks_to_groups, it->data, node);
    }

    return tasks_to_groups;
}

static UfoGraph *
build_group_graph (UfoBaseScheduler *scheduler, UfoTaskGraph *graph, UfoResources *resources, GError **error)
{
    UfoGraph *result;
    GList *nodes;
    GList *it;
    GHashTable *tasks_to_groups;

    result = ufo_graph_new ();
    nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));
    tasks_to_groups = build_task_groups (scheduler, graph, resources, nodes);

    /* Link groups */
    g_list_for (nodes, it) {
        GList *predecessors;
        GList *jt;
        TaskGroup *group;
        UfoNode *group_node;

        group_node = g_hash_table_lookup (tasks_to_groups, it->data);
        group = ufo_node_get_label (group_node);
        predecessors = ufo_graph_get_predecessors (UFO_GRAPH (graph), group->tasks->data);

        /* Connect predecessors to current node */
        g_list_for (predecessors, jt) {
            UfoNode *parent_node;
            UfoGroup *parent_group;

            parent_node = g_hash_table_lookup (tasks_to_groups, jt->data);
            parent_group = ufo_node_get_label (parent_node);
            group->parents = g_list_append (group->parents, parent_group);

            /* FIXME: use correct input as label */
            ufo_graph_connect_nodes (result, parent_node, group_node, NULL);
        }

        g_list_free (predecessors);
    }

    g_list_free (nodes);
    g_hash_table_destroy (tasks_to_groups);

    if (!expand_group_graph (resources, result, error)) {
        g_object_unref (result);
        return NULL;
    }

    return result;
}

static gboolean
pop_input_data (GList *parents, UfoBuffer **inputs)
{
    GList *it;
    guint i = 0;

    g_list_for (parents, it) {
        UfoTwoWayQueue *queue;

        queue = ((TaskGroup *) it->data)->queue;
        inputs[i] = ufo_two_way_queue_consumer_pop (queue);

        if (inputs[i] == POISON_PILL)
            return FALSE;

        i++;
    }

    return TRUE;
}

static void
release_input_data (GList *parents, UfoBuffer **inputs)
{
    GList *it;
    guint i = 0;

    g_list_for (parents, it) {
        UfoTwoWayQueue *queue;

        queue = ((TaskGroup *) it->data)->queue;
        ufo_two_way_queue_consumer_push (queue, inputs[i++]);
    }
}

static GList *
schedule_next (TaskGroup *group, GList *current)
{
    GList *next = NULL;

    switch (group->mode) {
        case TASK_GROUP_ROUND_ROBIN:
            next = g_list_next (current);

            if (next == NULL)
                next = g_list_first (group->tasks);

            break;

        case TASK_GROUP_RANDOM:
            next = g_list_nth (group->tasks,
                               g_random_int_range (0, g_list_length (group->tasks)));
            break;

        case TASK_GROUP_SHARED:
            next = g_list_first (group->tasks);
            break;
    }

    return next;
}

static gboolean
run_generator_or_processor (UfoTask *task,
                            UfoTaskMode mode,
                            UfoRequisition *requisition,
                            UfoBuffer **inputs,
                            UfoBuffer *output)
{
    gboolean active;

    switch (mode) {
        case UFO_TASK_MODE_PROCESSOR:
            active = ufo_task_process (task, inputs, output, requisition);
            break;

        case UFO_TASK_MODE_GENERATOR:
            active = ufo_task_generate (task, output, requisition);
            break;

        default:
            active = FALSE;
            break;
    }

    return active;
}

static GError *
run_group (TaskGroup *group)
{
    guint n_inputs;
    UfoBuffer **inputs;
    UfoBuffer *output;
    UfoRequisition requisition;
    UfoTask *task;
    UfoTaskMode mode;
    gboolean shared;
    gboolean active = TRUE;
    GList *current = NULL;  /* current task */
    GError *error = NULL;

    /* We should use get_structure to assert constraints ... */
    n_inputs = g_list_length (group->parents);
    inputs = g_new0 (UfoBuffer *, n_inputs);
    task = UFO_TASK (g_list_nth_data (group->tasks, 0));
    mode = ufo_task_get_mode (task) & UFO_TASK_MODE_TYPE_MASK;
    shared = ufo_task_get_mode (task) & UFO_TASK_MODE_SHARE_DATA;

    output = NULL;
    requisition.n_dims = 0;

    while (active) {
        /* Fetch data from parent groups */
        active = pop_input_data (group->parents, inputs);

        if (!active) {
            ufo_task_inputs_stopped_callback (task);
            break;
        }

        /* Choose next task of the group */
        current = schedule_next (group, current);

        task = UFO_TASK (current->data);

        /* Ask current task about size requirements */
        ufo_task_get_requisition (task, inputs, &requisition, &error);

        /* Insert output buffers as longs as capacity is not filled */
        if (!group->is_leaf) {
            if (ufo_two_way_queue_get_capacity (group->queue) < 2) {
                UfoBuffer *buffer;

                buffer = ufo_buffer_new (&requisition, group->context);
                ufo_two_way_queue_insert (group->queue, buffer);
            }

            output = ufo_two_way_queue_producer_pop (group->queue);
        }

        /* Generate/process the data. Because the functions return active state,
         * we negate it for the finished flag. */

        if (mode != UFO_TASK_MODE_REDUCTOR) {
            if (!shared) {
                active = run_generator_or_processor (task, mode, &requisition, inputs, output);
            }
            else {
                GList *it;

                g_list_for (group->tasks, it) {
                    task = UFO_TASK (it->data);
                    active = run_generator_or_processor (task, mode, &requisition, inputs, output);
                }
            }

            if (output != NULL && active) {
                ufo_two_way_queue_producer_push (group->queue, output);
            }

            release_input_data (group->parents, inputs);
        }
        else {
            /* This branch handles the reductor mode */

            do {
                active = ufo_task_process (task, inputs, output, &requisition);
                release_input_data (group->parents, inputs);
                active = pop_input_data (group->parents, inputs);
                if (!active) {
                    ufo_task_inputs_stopped_callback (task);
                }
            } while (active);

            /* Generate and forward as long as reductor produces data */
            do {
                active = ufo_task_generate (task, output, &requisition);

                if (active) {
                    ufo_two_way_queue_producer_push (group->queue, output);
                    output = ufo_two_way_queue_producer_pop (group->queue);
                }
            } while (active);
        }
    }

    if (!group->is_leaf)
        ufo_two_way_queue_producer_push (group->queue, POISON_PILL);

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

static void
ufo_group_scheduler_run (UfoBaseScheduler *scheduler,
                         UfoTaskGraph *task_graph,
                         GError **error)
{
    UfoResources *resources;
    UfoGraph *group_graph;
    GList *threads;
    GList *groups;
    GList *tasks;
    GList *it;

    g_return_if_fail (UFO_IS_GROUP_SCHEDULER (scheduler));

    resources = ufo_base_scheduler_get_resources (scheduler, error);

    if (resources == NULL)
        return;

    group_graph = build_group_graph (scheduler, task_graph, resources, error);

    if (group_graph == NULL)
        return;

    groups = ufo_graph_get_nodes (group_graph);
    threads = NULL;
    tasks = NULL;

    g_list_for (groups, it) {
        GThread *thread;
        GList *jt;
        TaskGroup *group;

        group = ufo_node_get_label (UFO_NODE (it->data));

        /* Setup each task in a group */
        g_list_for (group->tasks, jt) {
            UfoTaskNode *task = UFO_TASK_NODE (jt->data);

            ufo_task_setup (UFO_TASK (task), resources, error);

            tasks = g_list_append (tasks, task);

            if (error && *error)
                goto cleanup_run;
        }

        /* Run this group */
        thread = g_thread_new (NULL, (GThreadFunc) run_group, group);
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

cleanup_run:
    g_list_free (tasks);
    g_list_free (groups);

    g_object_unref (group_graph);
}

static void
ufo_group_scheduler_class_init (UfoGroupSchedulerClass *klass)
{
    UfoBaseSchedulerClass *sclass;

    sclass = UFO_BASE_SCHEDULER_CLASS (klass);
    sclass->run = ufo_group_scheduler_run;
}

static void
ufo_group_scheduler_init (UfoGroupScheduler *scheduler)
{
}
