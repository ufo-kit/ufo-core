/**
 * SECTION:ufo-arch-graph
 * @Short_description: Describe and hold #UfoGpuNode, #UfoCpuNode and
 * #UfoRemoteNode
 * @Title: UfoArchGraph
 */

#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <sched.h>
#include <ufo-arch-graph.h>
#include <ufo-cpu-node.h>
#include <ufo-gpu-node.h>
#include <ufo-remote-node.h>

G_DEFINE_TYPE (UfoArchGraph, ufo_arch_graph, UFO_TYPE_GRAPH)

#define UFO_ARCH_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ARCH_GRAPH, UfoArchGraphPrivate))

struct _UfoArchGraphPrivate {
    gpointer zmq_context;
    gpointer ocl_context;
    guint n_cpus;
    guint n_gpus;
};

/**
 * ufo_arch_graph_new:
 * @zmq_context: Context created with zmq_context_new().
 * @remote_addresses: (element-type utf8): A #GList containing address strings.
 * @resources: An initialized #UfoResources object
 *
 * Returns: A new #UfoArchGraph.
 */
UfoGraph *
ufo_arch_graph_new (gpointer zmq_context,
                    GList *remote_addresses,
                    UfoResources *resources)
{
    UfoArchGraph *graph;
    UfoArchGraphPrivate *priv;
    GList *cmd_queues;
    UfoNode **cpu_nodes;
    UfoNode **gpu_nodes;
    cpu_set_t mask;
    UfoNode **remote_nodes = NULL;
    guint n_remotes = 0;

    graph = UFO_ARCH_GRAPH (g_object_new (UFO_TYPE_ARCH_GRAPH, NULL));
    priv = graph->priv;
    priv->ocl_context = ufo_resources_get_context (resources);
    priv->zmq_context = zmq_context;

    /* Create CPU nodes */
    priv->n_cpus = (guint) get_nprocs ();
    cpu_nodes = g_new0 (UfoNode *, priv->n_cpus);

    for (guint i = 0; i < priv->n_cpus; i++) {
        CPU_ZERO (&mask);
        CPU_SET (i, &mask);
        cpu_nodes[i] = ufo_cpu_node_new (&mask);
    }

    /* Create GPU nodes, each one is associated with its own command queue. */
    cmd_queues = ufo_resources_get_cmd_queues (resources);
    priv->n_gpus = g_list_length (cmd_queues);
    gpu_nodes = g_new0 (UfoNode *, priv->n_gpus);

    for (guint i = 0; i < priv->n_gpus; i++) {
        gpu_nodes[i] = ufo_gpu_node_new (g_list_nth_data (cmd_queues, i));
    }

    /* Create remote nodes */
    if (zmq_context) {
        n_remotes = g_list_length (remote_addresses);
        remote_nodes = g_new0 (UfoNode *, n_remotes);

        for (guint i = 0; i < n_remotes; i++)
            remote_nodes[i] = ufo_remote_node_new (zmq_context,
                                                   (gchar *) g_list_nth_data (remote_addresses, i));
    }

    /*
     * Connect all CPUs to all GPUs. In the future this is the place for a
     * NUMA-specific mapping.
     */
    for (guint i = 0; i < priv->n_cpus; i++) {
        for (guint j = 0; j < priv->n_gpus; j++) {
            ufo_graph_connect_nodes (UFO_GRAPH (graph),
                                     cpu_nodes[i],
                                     gpu_nodes[j],
                                     NULL);
        }

        for (guint j = 0; j < n_remotes; j++) {
            ufo_graph_connect_nodes (UFO_GRAPH (graph),
                                     cpu_nodes[i],
                                     remote_nodes[j],
                                     NULL);
        }
    }

    g_free (cpu_nodes);
    g_free (gpu_nodes);
    g_free (remote_nodes);
    g_list_free (cmd_queues);
    return UFO_GRAPH (graph);
}

/**
 * ufo_arch_graph_get_context:
 * @graph: A #UfoArchGraph object
 *
 * Returns: OpenCL context associated with @graph.
 */
gpointer
ufo_arch_graph_get_context (UfoArchGraph *graph)
{
    g_return_val_if_fail (UFO_IS_ARCH_GRAPH (graph), NULL);
    return graph->priv->ocl_context;
}

/**
 * ufo_arch_graph_get_num_cpus:
 * @graph: A #UfoArchGraph object
 *
 * Returns: Number of CPU nodes in @graph.
 */
guint
ufo_arch_graph_get_num_cpus (UfoArchGraph *graph)
{
    g_return_val_if_fail (UFO_IS_ARCH_GRAPH (graph), 0);
    return graph->priv->n_cpus;
}

/**
 * ufo_arch_graph_get_num_gpus:
 * @graph: A #UfoArchGraph object
 *
 * Returns: Number of GPU nodes in @graph.
 */
guint
ufo_arch_graph_get_num_gpus (UfoArchGraph *graph)
{
    g_return_val_if_fail (UFO_IS_ARCH_GRAPH (graph), 0);
    return graph->priv->n_gpus;
}

static gboolean
is_gpu_node (UfoNode *node)
{
    return UFO_IS_GPU_NODE (node);
}

/**
 * ufo_arch_graph_get_gpu_nodes:
 * @graph: A #UfoArchGraph
 *
 * Returns: (element-type UfoGpuNode) (transfer container): A list of
 * #UfoGpuNode elements in @graph.
 */
GList *
ufo_arch_graph_get_gpu_nodes (UfoArchGraph *graph)
{
    return ufo_graph_get_nodes_filtered (UFO_GRAPH (graph),
                                         is_gpu_node);
}

static gboolean
is_remote_node (UfoNode *node)
{
    return UFO_IS_REMOTE_NODE (node);
}

/**
 * ufo_arch_graph_get_remote_nodes:
 * @graph: A #UfoArchGraph
 *
 * Returns: (element-type UfoGpuNode) (transfer container): A list of
 * #UfoRemoteNode elements in @graph.
 */
GList *
ufo_arch_graph_get_remote_nodes (UfoArchGraph *graph)
{
    return ufo_graph_get_nodes_filtered (UFO_GRAPH (graph),
                                         is_remote_node);
}

static void
ufo_arch_graph_class_init (UfoArchGraphClass *klass)
{
    g_type_class_add_private(klass, sizeof(UfoArchGraphPrivate));
}

static void
ufo_arch_graph_init (UfoArchGraph *self)
{
    UfoArchGraphPrivate *priv;
    self->priv = priv = UFO_ARCH_GRAPH_GET_PRIVATE (self);

    ufo_graph_register_node_type (UFO_GRAPH (self), UFO_TYPE_CPU_NODE);
    ufo_graph_register_node_type (UFO_GRAPH (self), UFO_TYPE_GPU_NODE);
    ufo_graph_register_node_type (UFO_GRAPH (self), UFO_TYPE_REMOTE_NODE);
}
