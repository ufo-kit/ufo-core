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
#include "ufo-scheduler.h"
#include "ufo-task-node.h"
#include "ufo-task-iface.h"
#include "ufo-priv.h"

/**
 * SECTION:ufo-scheduler
 * @Short_description: Expansion-based scheduler
 * @Title: UfoScheduler
 *
 * A scheduler that automatically distributes data according to an expansion
 * policy among different hardware resources. For that, paths of large work are
 * duplicated inside the #UfoTaskGraph and assigned to distinct GPUs.
 */

G_DEFINE_TYPE (UfoScheduler, ufo_scheduler, UFO_TYPE_BASE_SCHEDULER)

#define UFO_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SCHEDULER, UfoSchedulerPrivate))

typedef struct {
    UfoTask         *task;
    UfoTaskMode      mode;
    guint            n_inputs;
    guint           *dims;
    gboolean        *finished;
    gboolean         strict;
    gboolean         timestamps;
} TaskLocalData;


struct _UfoSchedulerPrivate {
    gboolean ran;
};


/**
 * UfoSchedulerError:
 * @UFO_SCHEDULER_ERROR_SETUP: Could not start scheduler due to error
 */
GQuark
ufo_scheduler_error_quark (void)
{
    return g_quark_from_static_string ("ufo-scheduler-error-quark");
}

/**
 * ufo_scheduler_new:
 *
 * Creates a new #UfoBaseScheduler.
 *
 * Return value: A new #UfoBaseScheduler
 */
UfoBaseScheduler *
ufo_scheduler_new (void)
{
    return UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_SCHEDULER, NULL));
}

static gboolean
get_inputs (TaskLocalData *tld,
            UfoBuffer **inputs)
{
    UfoRequisition req;
    UfoTaskNode *node = UFO_TASK_NODE (tld->task);
    guint n_finished = 0;

    for (guint i = 0; i < tld->n_inputs; i++) {
        UfoGroup *group;

        if (!tld->finished[i]) {
            UfoBuffer *input;

            group = ufo_task_node_get_current_in_group (node, i);
            input = ufo_group_pop_input_buffer (group, tld->task);

            if (tld->strict && input != UFO_END_OF_STREAM) {
                ufo_buffer_get_requisition (input, &req);

                if (req.n_dims != tld->dims[i]) {
                    g_warning ("%s: buffer from input %i provides %i dimensions but expect %i dimensions",
                               G_OBJECT_TYPE_NAME (tld->task), i, req.n_dims, tld->dims[i]);
                    return FALSE;
                }
            }

            if (input == UFO_END_OF_STREAM) {
                tld->finished[i] = TRUE;
                n_finished++;
            }
            else
                inputs[i] = input;
        }
        else
            n_finished++;
    }

    return (tld->n_inputs == 0) || (n_finished < tld->n_inputs);
}

static void
release_inputs (TaskLocalData *tld,
                UfoBuffer **inputs)
{
    UfoTaskNode *node = UFO_TASK_NODE (tld->task);

    for (guint i = 0; i < tld->n_inputs; i++) {
        UfoGroup *group;

        group = ufo_task_node_get_current_in_group (node, i);
        ufo_group_push_input_buffer (group, tld->task, inputs[i]);
        ufo_task_node_switch_in_group (node, i);
    }
}

static gpointer
run_task (TaskLocalData *tld)
{
    UfoBuffer *inputs[tld->n_inputs];
    UfoBuffer *output;
    UfoTaskNode *node;
    UfoTaskMode mode;
    UfoGroup *group;
    UfoRequisition requisition;
    gboolean produces;
    gboolean active;
    GError *error;

    node = UFO_TASK_NODE (tld->task);
    active = TRUE;
    output = NULL;
    error = NULL;

    /* mode without CPU/GPU flag */
    mode = tld->mode & UFO_TASK_MODE_TYPE_MASK;
    produces = mode != UFO_TASK_MODE_SINK;
    group = ufo_task_node_get_out_group (node);

    while (active) {
        /* Get input buffers */
        active = get_inputs (tld, inputs);

        if (!active) {
            ufo_task_inputs_stopped_callback (tld->task);
            ufo_group_finish (group);
            break;
        }

        /* Get output buffers */
        ufo_task_get_requisition (tld->task, inputs, &requisition, &error);

        if (error != NULL)
            break;

        if (produces) {
            output = ufo_group_pop_output_buffer (group, &requisition);
            g_assert (output != NULL);
        }

        if (output != NULL) {
            ufo_buffer_discard_location (output);

            for (guint i = 0; i < tld->n_inputs; i++)
                ufo_buffer_copy_metadata (inputs[i], output);
        }

        switch (mode) {
            case UFO_TASK_MODE_PROCESSOR:
                ufo_buffer_set_layout (output, ufo_buffer_get_layout (inputs[0]));
                /* fall through */
            case UFO_TASK_MODE_SINK:
                active = ufo_task_process (tld->task, inputs, output, &requisition);
                break;

            case UFO_TASK_MODE_REDUCTOR:
                do {
                    gboolean go_on = TRUE;

                    do {
                        go_on = ufo_task_process (tld->task, inputs, output, &requisition);

                        release_inputs (tld, inputs);
                        active = get_inputs (tld, inputs);
                        if (!active) {
                            ufo_task_inputs_stopped_callback (tld->task);
                        }
                        go_on = go_on && active;
                    } while (go_on);

                    do {
                        go_on = ufo_task_generate (tld->task, output, &requisition);

                        if (go_on) {
                            ufo_group_push_output_buffer (group, output);
                            output = ufo_group_pop_output_buffer (group, &requisition);
                        }
                    } while (go_on);
                } while (active);

                break;

            case UFO_TASK_MODE_GENERATOR:
                {
                    if (tld->timestamps) {
                        GValue v = { 0, };

                        g_value_init (&v, G_TYPE_INT64);
                        g_value_set_int64 (&v, g_get_real_time ());
                        ufo_buffer_set_metadata (output, "ts", &v);
                    }

                    active = ufo_task_generate (tld->task, output, &requisition);

                }
                break;

            default:
                g_warning ("Invalid task mode: %i\n", mode);
        }

        if (active && produces && (mode != UFO_TASK_MODE_REDUCTOR))
            ufo_group_push_output_buffer (group, output);

        /* Release buffers for further consumption */
        if (active)
            release_inputs (tld, inputs);

        if (!active)
            ufo_group_finish (group);
    }

    if (error) {
        /* flush outstanding input data */
        if (mode != UFO_TASK_MODE_GENERATOR) {
            while (get_inputs (tld, inputs))
                release_inputs (tld, inputs);
        }

        ufo_group_finish (group);
    }

    return error;
}

static void
cleanup_task_local_data (TaskLocalData **tlds,
                         guint n)
{
    for (guint i = 0; i < n; i++) {
        TaskLocalData *tld = tlds[i];

        ufo_task_node_reset (UFO_TASK_NODE (tld->task));

        g_free (tld->dims);
        g_free (tld->finished);
        g_free (tld);
    }

    g_free (tlds);
}

static gboolean
check_target_connections (UfoTaskGraph *graph,
                          UfoNode *target,
                          guint n_inputs,
                          GError **error)
{
    GList *predecessors;
    GList *it;
    guint16 connection_bitmap;
    guint16 mask;
    gboolean result = TRUE;

    if (n_inputs == 0)
        return TRUE;

    predecessors = ufo_graph_get_predecessors (UFO_GRAPH (graph), target);
    connection_bitmap = 0;

    /* Check all edges and enable bit number for edge label */
    g_list_for (predecessors, it) {
        gpointer label;
        gint input;

        label = ufo_graph_get_edge_label (UFO_GRAPH (graph),
                                          UFO_NODE (it->data), target);
        input = GPOINTER_TO_INT (label);
        g_assert (input >= 0 && input < 16);
        connection_bitmap |= 1 << input;
    }

    mask = (1 << n_inputs) - 1;

    /* Check if mask matches what we have */
    if ((mask & connection_bitmap) != mask) {
        g_set_error (error, UFO_SCHEDULER_ERROR, UFO_SCHEDULER_ERROR_SETUP,
                     "Not all inputs of `%s' are connected",
                     ufo_task_node_get_plugin_name (UFO_TASK_NODE (target)));

        result = FALSE;
    }

    g_list_free (predecessors);
    return result;
}

static TaskLocalData **
setup_tasks (UfoBaseScheduler *scheduler,
             UfoTaskGraph *task_graph,
             GError **error)
{
    UfoResources *resources;
    TaskLocalData **tlds;
    GList *nodes;
    guint n_nodes;
    gboolean timestamps;
    gboolean tracing_enabled;

    resources = ufo_base_scheduler_get_resources (scheduler, error);

    if (resources == NULL)
        return NULL;

    g_object_get (scheduler,
                  "enable-tracing", &tracing_enabled,
                  "timestamps", &timestamps,
                  NULL);

    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    n_nodes = g_list_length (nodes);

    tlds = g_new0 (TaskLocalData *, n_nodes);

    for (guint i = 0; i < n_nodes; i++) {
        UfoNode *node;
        TaskLocalData *tld;

        node = g_list_nth_data (nodes, i);
        tld = g_new0 (TaskLocalData, 1);
        tld->task = UFO_TASK (node);
        tlds[i] = tld;

        ufo_task_setup (tld->task, resources, error);
        tld->mode = ufo_task_get_mode (tld->task);
        tld->n_inputs = ufo_task_get_num_inputs (tld->task);
        tld->dims = g_new0 (guint, tld->n_inputs);
        tld->timestamps = timestamps;

        /* TODO: make this configurable from outside */
        tld->strict = FALSE;

        for (guint j = 0; j < tld->n_inputs; j++)
            tld->dims[j] = ufo_task_get_num_dimensions (tld->task, j);

        if (!check_target_connections (task_graph, node, tld->n_inputs, error)) {
            return NULL;
        }

        tld->finished = g_new0 (gboolean, tld->n_inputs);

        if (error && *error != NULL) {
            return NULL;
        }
    }

    g_list_free (nodes);

    return tlds;
}

static GList *
setup_groups (UfoBaseScheduler *scheduler,
              UfoTaskGraph *task_graph,
              GError **error)
{
    UfoResources *resources;
    GList *groups;
    GList *nodes;
    GList *it;
    cl_context context;

    groups = NULL;
    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    resources = ufo_base_scheduler_get_resources (scheduler, error);

    if (resources == NULL)
        return NULL;

    context = ufo_resources_get_context (resources);

    g_list_for (nodes, it) {
        GList *successors;
        GList *jt;
        UfoNode *node;
        UfoGroup *group;
        UfoSendPattern pattern;

        node = UFO_NODE (it->data);
        successors = ufo_graph_get_successors (UFO_GRAPH (task_graph), node);
        pattern = ufo_task_node_get_send_pattern (UFO_TASK_NODE (node));

        group = ufo_group_new (successors, context, pattern);
        groups = g_list_append (groups, group);
        ufo_task_node_set_out_group (UFO_TASK_NODE (node), group);

        g_list_for (successors, jt) {
            UfoNode *target;
            gpointer label;
            guint input;

            target = UFO_NODE (jt->data);
            label = ufo_graph_get_edge_label (UFO_GRAPH (task_graph), node, target);
            input = (guint) GPOINTER_TO_INT (label);
            ufo_task_node_add_in_group (UFO_TASK_NODE (target), input, group);
            ufo_group_set_num_expected (group, UFO_TASK (target),
                                        ufo_task_node_get_num_expected (UFO_TASK_NODE (target),
                                                                        input));
        }

        g_list_free (successors);
    }

    g_list_free (nodes);
    return groups;
}

static gboolean
correct_connections (UfoTaskGraph *graph,
                     GError **error)
{
    GList *nodes;
    GList *it;
    gboolean result = TRUE;

    nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));

    g_list_for (nodes, it) {
        UfoTaskNode *node;
        UfoTaskMode mode;
        UfoGroup *group;

        node = UFO_TASK_NODE (it->data);
        mode = ufo_task_get_mode (UFO_TASK (node)) & UFO_TASK_MODE_TYPE_MASK;
        group = ufo_task_node_get_out_group (node);

        if (((mode == UFO_TASK_MODE_GENERATOR) || (mode == UFO_TASK_MODE_REDUCTOR)) &&
            ufo_group_get_num_targets (group) < 1) {
            g_set_error (error, UFO_SCHEDULER_ERROR, UFO_SCHEDULER_ERROR_SETUP,
                         "No outgoing node for `%s'",
                         ufo_task_node_get_identifier (node));
            result = FALSE;
            break;
        }
    }

    g_list_free (nodes);
    return result;
}

static void
propagate_partition (UfoTaskGraph *graph)
{
    GList *nodes;
    GList *it;
    guint idx;
    guint total;

    ufo_task_graph_get_partition (graph, &idx, &total);
    nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));

    g_list_for (nodes, it) {
        ufo_task_node_set_partition (UFO_TASK_NODE (it->data), idx, total);
    }

    g_list_free (nodes);
}

static void
join_threads (GThread **threads, guint n_threads, GError **error)
{
    GError *tmp_error = NULL;

    for (guint i = 0; i < n_threads; i++) {
        tmp_error = g_thread_join (threads[i]);

        if (tmp_error) {
            g_clear_error (error);
            g_propagate_error (error, tmp_error);
        }
    }
}

static void
ufo_scheduler_run (UfoBaseScheduler *scheduler,
                   UfoTaskGraph *task_graph,
                   GError **error)
{
    UfoSchedulerPrivate *priv;
    UfoResources *resources;
    UfoTaskGraph *graph;
    GList *gpu_nodes;
    GList *groups;
    guint n_nodes;
    GThread **threads;
    TaskLocalData **tlds;
    gboolean expand;

    priv = UFO_SCHEDULER_GET_PRIVATE (scheduler);

    g_object_get (scheduler, "expand", &expand, NULL);

    graph = task_graph;
    resources = ufo_base_scheduler_get_resources (scheduler, error);

    if (resources == NULL)
        return;

    gpu_nodes = ufo_resources_get_gpu_nodes (resources);

    if (expand) {
        if (!priv->ran)
            ufo_task_graph_expand (graph, resources, g_list_length (gpu_nodes));
        else
            g_debug ("Task graph already expanded, skipping.");
    }

    propagate_partition (graph);
    ufo_task_graph_map (graph, gpu_nodes);

    /* Prepare task structures */
    tlds = setup_tasks (scheduler, graph, error);

    if (tlds == NULL)
        return;

    groups = setup_groups (scheduler, graph, error);

    if (groups == NULL)
        return;

    if (!correct_connections (graph, error))
        return;

    n_nodes = ufo_graph_get_num_nodes (UFO_GRAPH (graph));
    threads = g_new0 (GThread *, n_nodes);

    /* Spawn threads */
    for (guint i = 0; i < n_nodes; i++) {
        threads[i] = g_thread_new (NULL, (GThreadFunc) run_task, tlds[i]);

        if (error && (*error != NULL))
            return;
    }

#ifdef WITH_PYTHON
    if (Py_IsInitialized ()) {
        PyGILState_STATE state = PyGILState_Ensure ();
        Py_BEGIN_ALLOW_THREADS

        join_threads (threads, n_nodes, error);

        Py_END_ALLOW_THREADS
        PyGILState_Release (state);
    }
    else {
        join_threads (threads, n_nodes, error);
    }
#else
    join_threads (threads, n_nodes, error);
#endif

    /* Cleanup */
    cleanup_task_local_data (tlds, n_nodes);
    g_list_foreach (groups, (GFunc) g_object_unref, NULL);
    g_list_free (groups);
    g_list_free (gpu_nodes);
    g_free (threads);

    priv->ran = TRUE;
}

static void
ufo_scheduler_class_init (UfoSchedulerClass *klass)
{
    UfoBaseSchedulerClass *sclass;

    sclass = UFO_BASE_SCHEDULER_CLASS (klass);
    sclass->run = ufo_scheduler_run;

    g_type_class_add_private (klass, sizeof (UfoSchedulerPrivate));
}

static void
ufo_scheduler_init (UfoScheduler *scheduler)
{
    UfoSchedulerPrivate *priv;

    scheduler->priv = priv = UFO_SCHEDULER_GET_PRIVATE (scheduler);
    priv->ran = FALSE;
}
