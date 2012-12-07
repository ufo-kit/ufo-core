/**
 * SECTION:ufo-scheduler
 * @Short_description: Schedule the execution of a graph of nodes
 * @Title: UfoScheduler
 *
 * A scheduler object uses a graphs information to schedule the contained nodes
 * on CPU and GPU hardware.
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <stdio.h>
#include <string.h>

#include <ufo-scheduler.h>
#include <ufo-buffer.h>
#include <ufo-task-node.h>
#include <ufo-task-iface.h>
#include <ufo-cpu-task-iface.h>
#include <ufo-gpu-task-iface.h>

G_DEFINE_TYPE (UfoScheduler, ufo_scheduler, G_TYPE_OBJECT)

#define UFO_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SCHEDULER, UfoSchedulerPrivate))

typedef struct {
    UfoTask         *task;
    UfoTaskMode      mode;
    guint            n_inputs;
    guint           *n_dims;
} ThreadLocalData;

struct _UfoSchedulerPrivate {
    gboolean split;
};

enum {
    PROP_0,
    PROP_SPLIT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ufo_scheduler_new:
 *
 * Creates a new #UfoScheduler.
 *
 * Return value: A new #UfoScheduler
 */
UfoScheduler *
ufo_scheduler_new (void)
{
    return UFO_SCHEDULER (g_object_new (UFO_TYPE_SCHEDULER, NULL));
}

/**
 * ufo_scheduler_set_task_split:
 * @scheduler: A #UfoScheduler
 * @split: %TRUE if task graph should be split
 *
 * Sets whether the task graph should be split before execution to increase
 * multi GPU performance.
 */
void
ufo_scheduler_set_task_split (UfoScheduler *scheduler,
                              gboolean split)
{
    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));
    g_object_set (G_OBJECT (scheduler), "split", split, NULL);
}

static gboolean
get_inputs (ThreadLocalData *tld,
            UfoBuffer **inputs)
{
    UfoTaskNode *node = UFO_TASK_NODE (tld->task);

    for (guint i = 0; i < tld->n_inputs; i++) {
        UfoGroup *group = ufo_task_node_get_current_in_group (node, i);

        inputs[i] = ufo_group_pop_input_buffer (group, tld->task);

        if (inputs[i] == UFO_END_OF_STREAM)
            return FALSE;
    }

    return TRUE;
}

static void
release_inputs (ThreadLocalData *tld,
                UfoBuffer **inputs)
{
    UfoTaskNode *node = UFO_TASK_NODE (tld->task);

    for (guint i = 0; i < tld->n_inputs; i++) {
        UfoGroup *group = ufo_task_node_get_current_in_group (node, i);
        ufo_group_push_input_buffer (group, tld->task, inputs[i]);
        ufo_task_node_switch_in_group (node, i);
    }
}

static gpointer
run_task (ThreadLocalData *tld)
{
    UfoBuffer *inputs[tld->n_inputs];
    UfoBuffer *output;
    UfoTaskNode *node;
    UfoRequisition requisition;
    gboolean active;

    node = UFO_TASK_NODE (tld->task);
    active = TRUE;
    output = NULL;

    while (active) {
        /* Get input buffers */
        active = get_inputs (tld, inputs);

        if (!active) {
            ufo_group_finish (ufo_task_node_get_out_group (node));
            break;
        }

        /* Get output buffers */
        ufo_task_get_requisition (tld->task, inputs, &requisition);

        if (requisition.n_dims > 0) {
            UfoGroup *group;

            group = ufo_task_node_get_out_group (node);
            output = ufo_group_pop_output_buffer (group, &requisition);
            g_assert (output != NULL);
        }

        /* Process */
        if (UFO_IS_GPU_TASK (tld->task)) {
            if (output != NULL)
                ufo_buffer_discard_location (output, UFO_LOCATION_HOST);

            active = ufo_gpu_task_process (UFO_GPU_TASK (tld->task),
                                           inputs, output,
                                           &requisition,
                                           UFO_GPU_NODE (ufo_task_node_get_proc_node (node)));
        }
        else if (UFO_IS_CPU_TASK (tld->task)) {
            if (output != NULL)
                ufo_buffer_discard_location (output, UFO_LOCATION_DEVICE);

            switch (tld->mode) {
                case UFO_TASK_MODE_SINGLE:
                    active = ufo_cpu_task_process (UFO_CPU_TASK (tld->task), inputs, output, &requisition);
                    break;

                case UFO_TASK_MODE_GENERATE:
                    {
                        do {
                            ufo_cpu_task_process (UFO_CPU_TASK (tld->task),
                                                  inputs,
                                                  output,
                                                  &requisition);
                            release_inputs (tld, inputs);
                            active = get_inputs (tld, inputs);
                        } while (active);
                    }
                    break;
                case UFO_TASK_MODE_REDUCE:
                    {
                        do {
                            ufo_cpu_task_process (UFO_CPU_TASK (tld->task),
                                                  inputs,
                                                  output,
                                                  &requisition);
                            release_inputs (tld, inputs);
                            active = get_inputs (tld, inputs);
                        } while (active);

                        ufo_cpu_task_reduce (UFO_CPU_TASK (tld->task),
                                             output,
                                             &requisition);
                    }
                    break;
            }
        }

        /* Release buffers for further consumption */
        release_inputs (tld, inputs);

        if (requisition.n_dims > 0) {
            UfoGroup *group = ufo_task_node_get_out_group (node);

            switch (tld->mode) {
                case UFO_TASK_MODE_SINGLE:
                    if (active)
                        ufo_group_push_output_buffer (group, output);
                    else
                        ufo_group_finish (group);
                    break;

                case UFO_TASK_MODE_REDUCE:
                    ufo_group_push_output_buffer (group, output);
                    ufo_group_finish (group);
                    break;

                case UFO_TASK_MODE_GENERATE:
                    {
                        do {
                            active = ufo_cpu_task_generate (UFO_CPU_TASK (tld->task),
                                                            output,
                                                            &requisition);
                            ufo_group_push_output_buffer (group, output);
                            output = ufo_group_pop_output_buffer (group, &requisition);
                        } while (active);

                        ufo_group_finish (group);
                    }
                    break;
            }
        }
    }

    return NULL;
}

void
ufo_scheduler_run (UfoScheduler *scheduler,
                   UfoArchGraph *arch_graph,
                   UfoTaskGraph *task_graph,
                   GError **error)
{
    UfoSchedulerPrivate *priv;
    UfoResources *resources;
    GList *nodes;
    guint n_nodes;
    GThread **threads;
    ThreadLocalData **tlds;
    GTimer *timer;
    gpointer context;

    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));
    priv = scheduler->priv;

    if (priv->split)
        ufo_task_graph_split (task_graph, arch_graph);

    ufo_task_graph_map (task_graph, arch_graph);

    context = ufo_arch_graph_get_context (arch_graph);
    resources = ufo_arch_graph_get_resources (arch_graph);

    n_nodes = ufo_graph_get_num_nodes (UFO_GRAPH (task_graph));
    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));
    threads = g_new0 (GThread *, n_nodes);
    tlds = g_new0 (ThreadLocalData *, n_nodes);

    if (error && (*error != NULL))
        return;

    for (guint i = 0; i < n_nodes; i++) {
        UfoNode *node;
        UfoGroup *group;
        ThreadLocalData *tld;

        node = g_list_nth_data (nodes, i);

        /* Setup thread data */
        tld = g_new0 (ThreadLocalData, 1);
        tld->task = UFO_TASK (node);
        tlds[i] = tld;

        ufo_task_setup (UFO_TASK (node), resources, error);
        ufo_task_get_structure (UFO_TASK (node),
                                &tld->n_inputs,
                                &tld->n_dims,
                                &tld->mode);

        group = ufo_group_new (ufo_graph_get_successors (UFO_GRAPH (task_graph), node),
                               context);

        ufo_task_node_set_out_group (UFO_TASK_NODE (node), group);

        /* TODO: cleanup */
        if (error && *error != NULL)
            return;

        g_object_set_data (G_OBJECT (node), "thread-local-data", tld);
    }

    /* Connect group references */
    for (guint i = 0; i < n_nodes; i++) {
        UfoNode *node;
        UfoGroup *group;
        GList *successors;

        node = g_list_nth_data (nodes, i);
        group = ufo_task_node_get_out_group (UFO_TASK_NODE (node));
        successors = ufo_graph_get_successors (UFO_GRAPH (task_graph), node);

        for (GList *it = g_list_first (successors); it != NULL; it = g_list_next (it)) {
            UfoNode *target;
            gpointer label;
            guint input;

            target = UFO_NODE (it->data);
            label = ufo_graph_get_edge_label (UFO_GRAPH (task_graph), node, target);
            input = (guint) GPOINTER_TO_INT (label);
            ufo_task_node_add_in_group (UFO_TASK_NODE (target), input, group);
        }

        g_list_free (successors);
    }

    timer = g_timer_new ();

    /* Spawn threads */
    for (guint i = 0; i < n_nodes; i++) {
        threads[i] = g_thread_create ((GThreadFunc) run_task,
                                      tlds[i],
                                      TRUE, error);
        if (error && (*error != NULL))
            return;
    }

    /* Wait for threads to finish */
    for (guint i = 0; i < n_nodes; i++)
        g_thread_join (threads[i]);

    g_timer_stop (timer);
    g_print ("Processing finished after %3.5fs\n",
             g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);

    /* Cleanup */
    for (guint i = 0; i < n_nodes; i++) {
        UfoNode *node;
        UfoGroup *group;
        ThreadLocalData *tld;

        node = g_list_nth_data (nodes, i);
        group = ufo_task_node_get_out_group (UFO_TASK_NODE (node));
        g_object_unref (group);

        tld = tlds[i];
        g_free (tld);
    }

    g_free (tlds);
}

static void
ufo_scheduler_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    UfoSchedulerPrivate *priv = UFO_SCHEDULER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SPLIT:
            priv->split = g_value_get_boolean (value);
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
        case PROP_SPLIT:
            g_value_set_boolean (value, priv->split);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_scheduler_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_scheduler_parent_class)->dispose (object);
}

static void
ufo_scheduler_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_scheduler_parent_class)->finalize (object);
}

static void
ufo_scheduler_class_init (UfoSchedulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = ufo_scheduler_set_property;
    gobject_class->get_property = ufo_scheduler_get_property;
    gobject_class->dispose      = ufo_scheduler_dispose;
    gobject_class->finalize     = ufo_scheduler_finalize;

    properties[PROP_SPLIT] =
        g_param_spec_boolean ("split",
                              "Split the task graph for better multi GPU support",
                              "Split the task graph for better multi GPU support",
                              TRUE,
                              G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (klass, sizeof (UfoSchedulerPrivate));
}

static void
ufo_scheduler_init (UfoScheduler *scheduler)
{
    UfoSchedulerPrivate *priv;

    scheduler->priv = priv = UFO_SCHEDULER_GET_PRIVATE (scheduler);
    priv->split = TRUE;
}
