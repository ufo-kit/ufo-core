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

#ifdef HAVE_PYTHON
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
#include <ufo/ufo-cpu-task-iface.h>
#include <ufo/ufo-gpu-task-iface.h>
#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-remote-task.h>
#include <ufo/ufo-resources.h>
#include <ufo/ufo-scheduler.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-task-iface.h>
#include "compat.h"

/**
 * SECTION:ufo-scheduler
 * @Short_description: Schedule the execution of a graph of nodes
 * @Title: UfoScheduler
 *
 * A scheduler object uses a graphs information to schedule the contained nodes
 * on CPU and GPU hardware.
 */

static void ufo_scheduler_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoScheduler, ufo_scheduler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                ufo_scheduler_initable_iface_init))

#define UFO_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SCHEDULER, UfoSchedulerPrivate))

typedef struct {
    UfoTask         *task;
    UfoTaskMode      mode;
    guint            n_inputs;
    UfoInputParam   *in_params;
    gboolean        *finished;
} TaskLocalData;

struct _UfoSchedulerPrivate {
    GError          *construct_error;
    UfoConfig       *config;
    UfoResources    *resources;
    UfoArchGraph    *arch_graph;
    GList           *remotes;
    UfoRemoteMode    mode;
    gboolean         expand;
    gboolean         trace;
    gdouble          time;
};

enum {
    PROP_0,
    PROP_EXPAND,
    PROP_REMOTES,
    PROP_ENABLE_TRACING,
    PROP_TIME,
    N_PROPERTIES,

    /* Here come the overriden properties that we don't install ourselves. */
    PROP_CONFIG,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

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
 * @config: A #UfoConfig or %NULL
 * @remotes: (element-type utf8): A #GList with strings describing remote machines or %NULL
 *
 * Creates a new #UfoScheduler.
 *
 * Return value: A new #UfoScheduler
 */
UfoScheduler *
ufo_scheduler_new (UfoConfig *config,
                   GList *remotes)
{
    UfoScheduler *sched;
    UfoSchedulerPrivate *priv;
    GList *it;

    sched = UFO_SCHEDULER (g_object_new (UFO_TYPE_SCHEDULER,
                                         "config", config,
                                         NULL));
    priv = sched->priv;

    g_list_for (remotes, it) {
        priv->remotes = g_list_append (priv->remotes, g_strdup (it->data));
    }

    return sched;
}

/**
 * ufo_scheduler_get_context:
 * @scheduler: A #UfoScheduler
 *
 * Get the associated OpenCL context of @scheduler.
 *
 * Return value: (transfer full): An cl_context structure or %NULL on error.
 */
gpointer
ufo_scheduler_get_context (UfoScheduler *scheduler)
{
    g_return_val_if_fail (UFO_IS_SCHEDULER (scheduler), NULL);
    return ufo_resources_get_context (scheduler->priv->resources);
}

/**
 * ufo_scheduler_set_task_expansion:
 * @scheduler: A #UfoScheduler
 * @split: %TRUE if task graph should be split
 *
 * Sets whether the task graph should be expanded to accomodate for a multi GPU
 * system. Each suitable branch will be run on another GPU.
 */
void
ufo_scheduler_set_task_expansion (UfoScheduler *scheduler,
                                  gboolean expand)
{
    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));
    g_object_set (G_OBJECT (scheduler), "expand", expand, NULL);
}

/**
 * ufo_scheduler_set_remote_mode:
 * @scheduler: A #UfoScheduler
 * @mode: Mode of remote execution.
 *
 * Sets the mode of remote execution.
 *
 * See: #UfoRemoteMode.
 */
void
ufo_scheduler_set_remote_mode (UfoScheduler *scheduler,
                               UfoRemoteMode mode)
{
    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));
    scheduler->priv->mode = mode;
}

void
ufo_scheduler_set_arch_graph (UfoScheduler *scheduler,
                              UfoArchGraph *graph)
{
    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));

    if (scheduler->priv->arch_graph != NULL)
        g_object_unref (scheduler->priv->arch_graph);

    scheduler->priv->arch_graph = g_object_ref (graph);
}

/**
 * ufo_scheduler_get_resources:
 * @scheduler: A #UfoScheduler
 *
 * Get a reference on the #UfoResources object of this scheduler.
 *
 * Return value: (transfer none): Associated #UfoResources object.
 */
UfoResources *
ufo_scheduler_get_resources (UfoScheduler *scheduler)
{
    g_return_val_if_fail (UFO_IS_SCHEDULER (scheduler), NULL);
    return scheduler->priv->resources;
}

static gboolean
get_inputs (TaskLocalData *tld,
            UfoBuffer **inputs)
{
    UfoTaskNode *node = UFO_TASK_NODE (tld->task);
    guint n_finished = 0;

    for (guint i = 0; i < tld->n_inputs; i++) {
        UfoGroup *group;

        if (!tld->finished[i]) {
            UfoBuffer *input;

            group = ufo_task_node_get_current_in_group (node, i);
            input = ufo_group_pop_input_buffer (group, tld->task);

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

static gboolean
any (gboolean *values,
     guint n_values)
{
    gboolean result = FALSE;

    for (guint i = 0; i < n_values; i++)
        result = result || values[i];

    return result;
}

static void
run_remote_task (TaskLocalData *tld)
{
    UfoRemoteNode *remote;
    guint n_remote_gpus;
    gboolean *alive;
    gboolean active = TRUE;

    g_assert (tld->n_inputs == 1);

    remote = UFO_REMOTE_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (tld->task)));
    n_remote_gpus = ufo_remote_node_get_num_gpus (remote);
    alive = g_new0 (gboolean, n_remote_gpus);

    /*
     * We launch a new thread for each incoming input data set because then we
     * can send as many items as we have remote GPUs available without waiting
     * for processing to stop.
     */
    while (active) {
        for (guint i = 0; i < n_remote_gpus; i++) {
            UfoBuffer *input;

            if (get_inputs (tld, &input)) {
                ufo_remote_node_send_inputs (remote, &input);
                release_inputs (tld, &input);
                alive[i] = TRUE;
            }
            else {
                alive[i] = FALSE;
            }
        }

        for (guint i = 0; i < n_remote_gpus; i++) {
            UfoGroup *group;
            UfoBuffer *output;
            UfoRequisition requisition;

            if (!alive[i])
                continue;

            ufo_remote_node_get_requisition (remote, &requisition);
            group = ufo_task_node_get_out_group (UFO_TASK_NODE (tld->task));
            output = ufo_group_pop_output_buffer (group, &requisition);
            ufo_remote_node_get_result (remote, output);
            ufo_group_push_output_buffer (group, output);
        }

        active = any (alive, n_remote_gpus);
    }

    g_free (alive);
    ufo_group_finish (ufo_task_node_get_out_group (UFO_TASK_NODE (tld->task)));
}

static gboolean
check_implementation (UfoTaskNode *node,
                      const gchar *func,
                      const gchar *type,
                      gboolean assertion)
{
    if (!assertion) {
        g_error ("%s is not implemented, although %s is a %s",
                 func, ufo_task_node_get_plugin_name (node), type);
    }

    return assertion;
}

static gboolean
is_correctly_implemented (UfoTaskNode *node,
                          UfoTaskMode mode,
                          UfoTaskProcessFunc process,
                          UfoTaskGenerateFunc generate)
{
    switch (mode) {
        case UFO_TASK_MODE_GENERATOR:
            return check_implementation (node, "generate", "generator", generate != NULL);

        case UFO_TASK_MODE_PROCESSOR:
            return check_implementation (node, "process", "processor", process != NULL);

        case UFO_TASK_MODE_REDUCTOR:
            return check_implementation (node, "process or generate", "reductor",
                                         (process != NULL) && (generate != NULL));
        default:
            g_error ("Unknown mode");
            return FALSE;
    }
}

static gpointer
run_task (TaskLocalData *tld)
{
    UfoBuffer *inputs[tld->n_inputs];
    UfoBuffer *output;
    UfoTaskNode *node;
    UfoProfiler *profiler;
    UfoTaskProcessFunc process;
    UfoTaskGenerateFunc generate;
    UfoRequisition requisition;
    gboolean active;

    node = UFO_TASK_NODE (tld->task);
    active = TRUE;
    output = NULL;
    profiler = g_object_ref (ufo_task_node_get_profiler (node));

    if (UFO_IS_REMOTE_TASK (tld->task)) {
        run_remote_task (tld);
        return NULL;
    }

    if (UFO_IS_GPU_TASK (tld->task)) {
        process = (UfoTaskProcessFunc) ufo_gpu_task_process;
        generate = (UfoTaskGenerateFunc) ufo_gpu_task_generate;
    }
    else {
        process = (UfoTaskProcessFunc) ufo_cpu_task_process;
        generate = (UfoTaskGenerateFunc) ufo_cpu_task_generate;
    }

    if (!is_correctly_implemented (node, tld->mode, process, generate))
        return NULL;

    while (active) {
        UfoGroup *group;
        gboolean produces;

        group = ufo_task_node_get_out_group (node);

        /* Get input buffers */
        active = get_inputs (tld, inputs);

        if (!active) {
            ufo_group_finish (group);
            break;
        }

        /* Get output buffers */
        ufo_task_get_requisition (tld->task, inputs, &requisition);
        produces = requisition.n_dims > 0;

        if (produces) {
            output = ufo_group_pop_output_buffer (group, &requisition);
            g_assert (output != NULL);
        }

        if (output != NULL)
            ufo_buffer_discard_location (output);

        switch (tld->mode) {
            case UFO_TASK_MODE_PROCESSOR:
                ufo_profiler_trace_event (profiler, "process", "B");
                active = process (tld->task, inputs, output, &requisition);
                ufo_profiler_trace_event (profiler, "process", "E");
                ufo_task_node_increase_processed (UFO_TASK_NODE (tld->task));
                break;

            case UFO_TASK_MODE_REDUCTOR:
                do {
                    ufo_profiler_trace_event (profiler, "process", "B");
                    process (tld->task, inputs, output, &requisition);
                    ufo_profiler_trace_event (profiler, "process", "E");
                    ufo_task_node_increase_processed (UFO_TASK_NODE (tld->task));

                    release_inputs (tld, inputs);
                    active = get_inputs (tld, inputs);
                } while (active);
                break;

            case UFO_TASK_MODE_GENERATOR:
                ufo_profiler_trace_event (profiler, "generate", "B");
                active = generate (tld->task, output, &requisition);
                ufo_profiler_trace_event (profiler, "generate", "E");
                break;
        }

        if (active && produces && (tld->mode != UFO_TASK_MODE_REDUCTOR))
            ufo_group_push_output_buffer (group, output);

        /* Release buffers for further consumption */
        if (active)
            release_inputs (tld, inputs);

        if (tld->mode == UFO_TASK_MODE_REDUCTOR) {
            active = TRUE;

            do {
                ufo_profiler_trace_event (profiler, "generate", "B");
                active = generate (tld->task, output, &requisition);
                ufo_profiler_trace_event (profiler, "generate", "E");

                if (active) {
                    ufo_group_push_output_buffer (group, output);
                    output = ufo_group_pop_output_buffer (group, &requisition);
                }
            } while (active);
        }

        if (!active)
            ufo_group_finish (group);
    }

    g_object_unref (profiler);
    return NULL;
}

static void
cleanup_task_local_data (TaskLocalData **tlds,
                         guint n)
{
    for (guint i = 0; i < n; i++) {
        TaskLocalData *tld = tlds[i];

        g_free (tld->in_params);
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
setup_tasks (UfoSchedulerPrivate *priv,
             UfoTaskGraph *task_graph,
             GError **error)
{
    TaskLocalData **tlds;
    GList *nodes;
    guint n_nodes;

    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    n_nodes = g_list_length (nodes);

    tlds = g_new0 (TaskLocalData *, n_nodes);

    for (guint i = 0; i < n_nodes; i++) {
        UfoNode *node;
        UfoProfiler *profiler;
        TaskLocalData *tld;

        node = g_list_nth_data (nodes, i);
        tld = g_new0 (TaskLocalData, 1);
        tld->task = UFO_TASK (node);
        tlds[i] = tld;

        ufo_task_setup (UFO_TASK (node), priv->resources, error);
        ufo_task_get_structure (UFO_TASK (node), &tld->n_inputs, &tld->in_params, &tld->mode);

        if (!check_target_connections (task_graph, node, tld->n_inputs, error))
            return NULL;

        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (node));
        ufo_profiler_enable_tracing (profiler, priv->trace);

        tld->finished = g_new0 (gboolean, tld->n_inputs);

        if (error && *error != NULL)
            return NULL;
    }

    g_list_free (nodes);
    return tlds;
}

static GList *
setup_groups (UfoSchedulerPrivate *priv,
              UfoTaskGraph *task_graph)
{
    GList *groups;
    GList *nodes;
    GList *it;
    cl_context context;

    groups = NULL;
    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    context = ufo_resources_get_context (priv->resources);

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
        UfoInputParam *in_params;
        guint n_inputs;
        UfoTaskMode mode;
        UfoGroup *group;

        node = UFO_TASK_NODE (it->data);
        ufo_task_get_structure (UFO_TASK (node), &n_inputs, &in_params, &mode);
        group = ufo_task_node_get_out_group (node);

        if (((mode == UFO_TASK_MODE_GENERATOR) || (mode == UFO_TASK_MODE_REDUCTOR)) &&
            ufo_group_get_num_targets (group) < 1) {
            g_set_error (error, UFO_SCHEDULER_ERROR, UFO_SCHEDULER_ERROR_SETUP,
                         "No outgoing node for `%s'",
                         ufo_task_node_get_unique_name (node));
            result = FALSE;
            break;
        }
    }

    g_list_free (nodes);
    return result;
}

static void
replicate_task_graph (UfoTaskGraph *graph,
                      UfoArchGraph *arch)
{
    GList *remotes;
    GList *it;
    guint n_graphs;
    guint idx = 1;

    remotes = ufo_arch_graph_get_remote_nodes (arch);
    n_graphs = g_list_length (remotes) + 1;

    g_list_for (remotes, it) {
        UfoRemoteNode *node;
        gchar *json;

        /* Set partition idx for the remote task graph */
        ufo_task_graph_set_partition (graph, idx++, n_graphs);
        json = ufo_task_graph_get_json_data (graph, NULL);
        node = UFO_REMOTE_NODE (it->data);
        ufo_remote_node_send_json (node, UFO_REMOTE_MODE_REPLICATE, json);
        g_free (json);
    }

    /* Set partition index for the local task graph */
    ufo_task_graph_set_partition (graph, 0, n_graphs);
    g_list_free (remotes);
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

typedef struct {
    UfoTraceEvent *event;
    UfoTaskNode *node;
    gdouble timestamp;
} SortedEvent;

static gint
compare_event (const SortedEvent *a,
               const SortedEvent *b,
               gpointer user_data)
{
    return (gint) (a->timestamp - b->timestamp);
}

static GList *
get_sorted_event_trace (TaskLocalData **tlds,
                        guint n_nodes)
{
    GList *sorted = NULL;

    for (guint i = 0; i < n_nodes; i++) {
        UfoTaskNode *node;
        UfoProfiler *profiler;
        GList *events;
        GList *it;

        node = UFO_TASK_NODE (tlds[i]->task);
        profiler = ufo_task_node_get_profiler (node);
        events = ufo_profiler_get_trace_events (profiler);

        g_list_for (events, it) {
            UfoTraceEvent *event;
            SortedEvent *new_event;

            event = (UfoTraceEvent *) it->data;
            new_event = g_new0 (SortedEvent, 1);
            new_event->event = event;
            new_event->timestamp = event->timestamp;
            new_event->node = node;
            sorted = g_list_insert_sorted_with_data (sorted, new_event,
                                                     (GCompareDataFunc) compare_event, NULL);
        }
    }

    return sorted;
}

static void
write_traces (TaskLocalData **tlds,
              guint n_nodes)
{
    FILE *fp;
    gchar *filename;
    guint pid;
    GList *sorted;
    GList *it;

    sorted = get_sorted_event_trace (tlds, n_nodes);
    pid = (guint) getpid ();
    filename = g_strdup_printf (".trace.%i.json", pid);
    fp = fopen (filename, "w");
    fprintf (fp, "{ \"traceEvents\": [");

    g_list_for (sorted, it) {
        SortedEvent *sorted_event;
        UfoTraceEvent *event;
        gchar *name;

        sorted_event = (SortedEvent *) it->data;
        event = sorted_event->event;
        name = g_strdup_printf ("%s-%p", G_OBJECT_TYPE_NAME (sorted_event->node),
                                (gpointer) sorted_event->node);

        fprintf (fp, "{\"cat\":\"f\",\"ph\": \"%s\", \"ts\": %.1f, \"pid\": %i, \"tid\": \"%s\",\"name\": \"%s\", \"args\": {}}\n",
                     event->type, event->timestamp, pid, name, event->name);
        if (g_list_next (it) != NULL)
            fprintf (fp, ",");

        g_free (name);
    }

    fprintf (fp, "] }");
    fclose (fp);

    g_list_foreach (sorted, (GFunc) g_free, NULL);
    g_list_free (sorted);
}

static void
join_threads (GThread **threads, guint n_threads)
{
    for (guint i = 0; i < n_threads; i++)
        g_thread_join (threads[i]);
}

void
ufo_scheduler_run (UfoScheduler *scheduler,
                   UfoTaskGraph *task_graph,
                   GError **error)
{
    UfoSchedulerPrivate *priv;
    UfoArchGraph *arch_graph;
    GList *groups;
    guint n_nodes;
    GThread **threads;
    TaskLocalData **tlds;
    GTimer *timer;

    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));
    priv = scheduler->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);
        return;
    }

    if (priv->arch_graph != NULL) {
        arch_graph = priv->arch_graph;
    }
    else {
        arch_graph = UFO_ARCH_GRAPH (ufo_arch_graph_new (priv->resources,
                                                         priv->remotes));
    }

    if (priv->mode == UFO_REMOTE_MODE_REPLICATE) {
        replicate_task_graph (task_graph, arch_graph);
    }

    if (priv->expand) {
        gboolean expand_remote = priv->mode == UFO_REMOTE_MODE_STREAM;
        ufo_task_graph_expand (task_graph, arch_graph, expand_remote);
    }

    propagate_partition (task_graph);
    ufo_task_graph_map (task_graph, arch_graph);

    /* Prepare task structures */
    tlds = setup_tasks (priv, task_graph, error);

    if (tlds == NULL)
        return;

    groups = setup_groups (priv, task_graph);

    if (!correct_connections (task_graph, error))
        return;

    n_nodes = ufo_graph_get_num_nodes (UFO_GRAPH (task_graph));
    threads = g_new0 (GThread *, n_nodes);
    timer = g_timer_new ();

    /* Spawn threads */
    for (guint i = 0; i < n_nodes; i++) {
        threads[i] = g_thread_create ((GThreadFunc) run_task, tlds[i], TRUE, error);

        if (error && (*error != NULL))
            return;
    }

#ifdef HAVE_PYTHON
    if (Py_IsInitialized ()) {
        Py_BEGIN_ALLOW_THREADS

        join_threads (threads, n_nodes);

        Py_END_ALLOW_THREADS
    }
    else {
        join_threads (threads, n_nodes);
    }
#else
    join_threads (threads, n_nodes);
#endif

#ifdef HAVE_PYTHON
    if (Py_IsInitialized ())
#endif

    priv->time = g_timer_elapsed (timer, NULL);
    g_message ("Processing finished after %3.5fs", priv->time);
    g_timer_destroy (timer);

    /* Cleanup */
    if (priv->trace)
        write_traces (tlds, n_nodes);

    cleanup_task_local_data (tlds, n_nodes);
    g_list_foreach (groups, (GFunc) g_object_unref, NULL);
    g_list_free (groups);
    g_free (threads);

    if (priv->arch_graph == NULL)
        g_object_unref (arch_graph);
}

static void
copy_remote_list (UfoSchedulerPrivate *priv,
                  GValueArray *array)
{
    if (priv->remotes != NULL) {
        g_list_foreach (priv->remotes, (GFunc) g_free, NULL);
        g_list_free (priv->remotes);
        priv->remotes = NULL;
    }

    for (guint i = 0; i < array->n_values; i++) {
        priv->remotes = g_list_append (priv->remotes,
                                       g_strdup (g_value_get_string (g_value_array_get_nth (array, i))));
    }
}

static void
ufo_scheduler_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    UfoSchedulerPrivate *priv = UFO_SCHEDULER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_CONFIG:
            {
                GObject *vobject = g_value_get_object (value);

                if (vobject != NULL) {
                    if (priv->config != NULL)
                        g_object_unref (priv->config);

                    priv->config = UFO_CONFIG (vobject);
                    g_object_ref (priv->config);
                }
            }
            break;

        case PROP_REMOTES:
            copy_remote_list (priv, g_value_get_boxed (value));
            break;

        case PROP_EXPAND:
            priv->expand = g_value_get_boolean (value);
            break;

        case PROP_ENABLE_TRACING:
            priv->trace = g_value_get_boolean (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_scheduler_get_property (GObject      *object,
                            guint         property_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    UfoSchedulerPrivate *priv = UFO_SCHEDULER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPAND:
            g_value_set_boolean (value, priv->expand);
            break;

        case PROP_ENABLE_TRACING:
            g_value_set_boolean (value, priv->trace);
            break;

        case PROP_TIME:
            g_value_set_double (value, priv->time);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_scheduler_constructed (GObject *object)
{
    UfoSchedulerPrivate *priv;

    priv = UFO_SCHEDULER_GET_PRIVATE (object);
    priv->resources = ufo_resources_new (priv->config,
                                         &priv->construct_error);
}

static void
ufo_scheduler_dispose (GObject *object)
{
    UfoSchedulerPrivate *priv;

    priv = UFO_SCHEDULER_GET_PRIVATE (object);

    if (priv->config != NULL) {
        g_object_unref (priv->config);
        priv->config = NULL;
    }

    if (priv->resources != NULL) {
        g_object_unref (priv->resources);
        priv->resources = NULL;
    }

    if (priv->arch_graph != NULL) {
        g_object_unref (priv->arch_graph);
        priv->arch_graph = NULL;
    }

    G_OBJECT_CLASS (ufo_scheduler_parent_class)->dispose (object);
}

static void
ufo_scheduler_finalize (GObject *object)
{
    UfoSchedulerPrivate *priv;

    priv = UFO_SCHEDULER_GET_PRIVATE (object);

    g_clear_error (&priv->construct_error);
    g_list_foreach (priv->remotes, (GFunc) g_free, NULL);
    g_list_free (priv->remotes);
    priv->remotes = NULL;

    G_OBJECT_CLASS (ufo_scheduler_parent_class)->finalize (object);
}

static gboolean
ufo_scheduler_initable_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
    UfoScheduler *scheduler;
    UfoSchedulerPrivate *priv;

    g_return_val_if_fail (UFO_IS_SCHEDULER (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    scheduler = UFO_SCHEDULER (initable);
    priv = scheduler->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);

        return FALSE;
    }

    return TRUE;
}

static void
ufo_scheduler_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_scheduler_initable_init;
}

static void
ufo_scheduler_class_init (UfoSchedulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->constructed  = ufo_scheduler_constructed;
    gobject_class->set_property = ufo_scheduler_set_property;
    gobject_class->get_property = ufo_scheduler_get_property;
    gobject_class->dispose      = ufo_scheduler_dispose;
    gobject_class->finalize     = ufo_scheduler_finalize;

    properties[PROP_EXPAND] =
        g_param_spec_boolean ("expand",
                              "Expand the task graph for better multi GPU performance",
                              "Expand the task graph for better multi GPU performance",
                              TRUE,
                              G_PARAM_READWRITE);

    properties[PROP_ENABLE_TRACING] =
        g_param_spec_boolean ("enable-tracing",
                              "Enable and write profile traces",
                              "Enable and write profile traces",
                              FALSE,
                              G_PARAM_READWRITE);

    properties[PROP_REMOTES] =
        g_param_spec_value_array ("remotes",
                                  "List containing remote addresses",
                                  "List containing remote addresses of machines running ufod",
                                  g_param_spec_string ("remote",
                                                       "A remote address in the form tcp://addr:port",
                                                       "A remote address in the form tcp://addr:port (see http://api.zeromq.org/3-2:zmq-tcp)",
                                                       ".",
                                                       G_PARAM_READWRITE),
                                  G_PARAM_READWRITE);

    properties[PROP_TIME] =
        g_param_spec_double ("time",
                             "Finished execution time",
                             "Finished execution time in seconds",
                              0.0, G_MAXDOUBLE, 0.0,
                              G_PARAM_READABLE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_object_class_override_property (gobject_class, PROP_CONFIG, "config");

    g_type_class_add_private (klass, sizeof (UfoSchedulerPrivate));
}

static void
ufo_scheduler_init (UfoScheduler *scheduler)
{
    UfoSchedulerPrivate *priv;

    scheduler->priv = priv = UFO_SCHEDULER_GET_PRIVATE (scheduler);
    priv->expand = TRUE;
    priv->trace = FALSE;
    priv->config = NULL;
    priv->resources = NULL;
    priv->arch_graph = NULL;
    priv->remotes = NULL;
    priv->construct_error = NULL;
    priv->mode = UFO_REMOTE_MODE_STREAM;
    priv->time = 0.0;
}
