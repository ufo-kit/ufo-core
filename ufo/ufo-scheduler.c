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

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
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

/**
 * SECTION:ufo-scheduler
 * @Short_description: Schedule the execution of a graph of nodes
 * @Title: UfoScheduler
 *
 * A scheduler object uses a graphs information to schedule the contained nodes
 * on CPU and GPU hardware.
 */

G_DEFINE_TYPE_WITH_CODE (UfoScheduler, ufo_scheduler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SCHEDULER, UfoSchedulerPrivate))

typedef struct {
    UfoTask         *task;
    UfoTaskMode      mode;
    guint            n_inputs;
    UfoInputParam   *in_params;
    gboolean        *finished;
} TaskLocalData;

struct _UfoSchedulerPrivate {
    UfoConfig       *config;
    UfoResources    *resources;
    GList           *remotes;
    UfoRemoteMode    mode;
    gboolean         expand;
};

enum {
    PROP_0,
    PROP_EXPAND,
    PROP_REMOTES,
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

    sched = UFO_SCHEDULER (g_object_new (UFO_TYPE_SCHEDULER,
                                         "config", config,
                                         NULL));
    priv = sched->priv;

    for (GList *it = g_list_first (remotes); it != NULL; it = g_list_next (it))
        priv->remotes = g_list_append (priv->remotes, g_strdup (it->data));

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

    profiler = ufo_profiler_new ();
    ufo_task_node_set_profiler (node, profiler);

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
                active = process (tld->task, inputs, output, &requisition);
                break;

            case UFO_TASK_MODE_REDUCTOR:
                do {
                    process (tld->task, inputs, output, &requisition);
                    release_inputs (tld, inputs);
                    active = get_inputs (tld, inputs);
                } while (active);
                break;

            case UFO_TASK_MODE_GENERATOR:
                active = generate (tld->task, output, &requisition);
                break;
        }

        if (active && produces && (tld->mode != UFO_TASK_MODE_REDUCTOR))
            ufo_group_push_output_buffer (group, output);

        /* Release buffers for further consumption */
        release_inputs (tld, inputs);

        if (tld->mode == UFO_TASK_MODE_REDUCTOR) {
            active = TRUE;

            do {
                active = generate (tld->task, output, &requisition);

                if (active) {
                    ufo_group_push_output_buffer (group, output);
                    output = ufo_group_pop_output_buffer (group, &requisition);
                }
            } while (active);
        }

        if (!active)
            ufo_group_finish (group);
    }

    g_message ("`%s' finished: CPU: %3.5fs GPU: %3.5fs IO: %3.5fs",
               G_OBJECT_TYPE_NAME (tld->task),
               ufo_profiler_elapsed (profiler, UFO_PROFILER_TIMER_CPU),
               ufo_profiler_elapsed (profiler, UFO_PROFILER_TIMER_GPU),
               ufo_profiler_elapsed (profiler, UFO_PROFILER_TIMER_IO));

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
        TaskLocalData *tld;

        node = g_list_nth_data (nodes, i);
        tld = g_new0 (TaskLocalData, 1);
        tld->task = UFO_TASK (node);
        tlds[i] = tld;

        ufo_task_setup (UFO_TASK (node), priv->resources, error);
        ufo_task_get_structure (UFO_TASK (node), &tld->n_inputs, &tld->in_params, &tld->mode);

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
    cl_context context;

    groups = NULL;
    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    context = ufo_resources_get_context (priv->resources);

    for (GList *it = g_list_first (nodes); it != NULL; it = g_list_next (it)) {
        GList *successors;
        UfoNode *node;
        UfoGroup *group;
        UfoSendPattern pattern;

        node = UFO_NODE (it->data);
        successors = ufo_graph_get_successors (UFO_GRAPH (task_graph), node);
        pattern = ufo_task_node_get_send_pattern (UFO_TASK_NODE (node));

        group = ufo_group_new (successors, context, pattern);
        groups = g_list_append (groups, group);
        ufo_task_node_set_out_group (UFO_TASK_NODE (node), group);

        for (GList *jt = g_list_first (successors); jt != NULL; jt = g_list_next (jt)) {
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
    gboolean result = TRUE;

    nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));

    for (GList *it = g_list_first (nodes); it != NULL; it = g_list_next (it)) {
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
    guint n_graphs;
    guint index = 1;

    remotes = ufo_arch_graph_get_remote_nodes (arch);
    n_graphs = g_list_length (remotes) + 1;

    for (GList *it = g_list_first (remotes); it != NULL; it = g_list_next (it)) {
        UfoRemoteNode *node;
        gchar *json;

        /* Set partition index for the remote task graph */
        ufo_task_graph_set_partition (graph, index++, n_graphs);
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
    guint index;
    guint total;

    ufo_task_graph_get_partition (graph, &index, &total);
    nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));

    for (GList *it = g_list_first (nodes); it != NULL; it = g_list_next (it))
        ufo_task_node_set_partition (UFO_TASK_NODE (it->data), index, total);

    g_list_free (nodes);
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

    arch_graph = UFO_ARCH_GRAPH (ufo_arch_graph_new (priv->resources,
                                                     priv->remotes));

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

    /* Wait for threads to finish */
    for (guint i = 0; i < n_nodes; i++)
        g_thread_join (threads[i]);

    g_print ("Processing finished after %3.5fs\n", g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);

    /* Cleanup */
    cleanup_task_local_data (tlds, n_nodes);
    g_list_foreach (groups, (GFunc) g_object_unref, NULL);
    g_list_free (groups);
    g_free (threads);

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
    priv->resources = ufo_resources_new (priv->config);
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

    G_OBJECT_CLASS (ufo_scheduler_parent_class)->dispose (object);
}

static void
ufo_scheduler_finalize (GObject *object)
{
    UfoSchedulerPrivate *priv;

    priv = UFO_SCHEDULER_GET_PRIVATE (object);

    g_list_foreach (priv->remotes, (GFunc) g_free, NULL);
    g_list_free (priv->remotes);
    priv->remotes = NULL;

    G_OBJECT_CLASS (ufo_scheduler_parent_class)->finalize (object);
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
    priv->config = NULL;
    priv->resources = NULL;
    priv->remotes = NULL;
    priv->mode = UFO_REMOTE_MODE_STREAM;
}
