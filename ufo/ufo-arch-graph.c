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

#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <sched.h>
#include <zmq.h>
#include <ufo/ufo-arch-graph.h>
#include <ufo/ufo-cpu-node.h>
#include <ufo/ufo-gpu-node.h>
#include <ufo/ufo-remote-node.h>

/**
 * SECTION:ufo-arch-graph
 * @Short_description: Describe and hold #UfoGpuNode, #UfoCpuNode and
 * #UfoRemoteNode
 * @Title: UfoArchGraph
 */

G_DEFINE_TYPE (UfoArchGraph, ufo_arch_graph, UFO_TYPE_GRAPH)

#define UFO_ARCH_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ARCH_GRAPH, UfoArchGraphPrivate))

struct _UfoArchGraphPrivate {
    UfoResources *resources;
    UfoNode **cpu_nodes;
    UfoNode **gpu_nodes;
    UfoNode **remote_nodes;
    guint n_cpus;
    guint n_gpus;
    guint n_remotes;
};

/**
 * ufo_arch_graph_new:
 * @resources: An initialized #UfoResources object
 * @remote_addresses: (element-type utf8): (allow-none): A #GList containing
 * address strings.
 *
 * Returns: A new #UfoArchGraph.
 */
UfoGraph *
ufo_arch_graph_new (UfoResources *resources,
                    GList *remote_addresses)
{
    UfoArchGraph *graph;
    UfoArchGraphPrivate *priv;
    GList *cmd_queues;
    cpu_set_t mask;

    graph = UFO_ARCH_GRAPH (g_object_new (UFO_TYPE_ARCH_GRAPH, NULL));
    priv = graph->priv;

    g_object_ref (resources);
    priv->resources = resources;

    /* Create CPU nodes */
    priv->n_cpus = (guint) get_nprocs ();
    priv->cpu_nodes = g_new0 (UfoNode *, priv->n_cpus);

    for (guint i = 0; i < priv->n_cpus; i++) {
        CPU_ZERO (&mask);
        CPU_SET (i, &mask);
        priv->cpu_nodes[i] = ufo_cpu_node_new (&mask);
    }

    /* Create GPU nodes, each one is associated with its own command queue. */
    cmd_queues = ufo_resources_get_cmd_queues (resources);
    priv->n_gpus = g_list_length (cmd_queues);
    priv->gpu_nodes = g_new0 (UfoNode *, priv->n_gpus);

    for (guint i = 0; i < priv->n_gpus; i++) {
        priv->gpu_nodes[i] = ufo_gpu_node_new (g_list_nth_data (cmd_queues, i));
    }

    /* Create remote nodes */
    priv->n_remotes = g_list_length (remote_addresses);

    if (priv->n_remotes > 0) {
        priv->remote_nodes = g_new0 (UfoNode *, priv->n_remotes);

        for (guint i = 0; i < priv->n_remotes; i++) {
            priv->remote_nodes[i] = ufo_remote_node_new ((gchar *) g_list_nth_data (remote_addresses, i));
        }
    }

    /*
     * Connect all CPUs to all GPUs. In the future this is the place for a
     * NUMA-specific mapping.
     */
    for (guint i = 0; i < priv->n_cpus; i++) {
        for (guint j = 0; j < priv->n_gpus; j++) {
            ufo_graph_connect_nodes (UFO_GRAPH (graph),
                                     priv->cpu_nodes[i],
                                     priv->gpu_nodes[j],
                                     NULL);
        }

        for (guint j = 0; j < priv->n_remotes; j++) {
            ufo_graph_connect_nodes (UFO_GRAPH (graph),
                                     priv->cpu_nodes[i],
                                     priv->remote_nodes[j],
                                     NULL);
        }
    }

    g_list_free (cmd_queues);
    return UFO_GRAPH (graph);
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

/**
 * ufo_arch_graph_get_num_remotes:
 * @graph: A #UfoArchGraph object
 *
 * Returns: Number of remote nodes in @graph.
 */
guint
ufo_arch_graph_get_num_remotes (UfoArchGraph *graph)
{
    g_return_val_if_fail (UFO_IS_ARCH_GRAPH (graph), 0);
    return graph->priv->n_remotes;
}

static gboolean
is_gpu_node (UfoNode *node, gpointer user_data)
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
                                         is_gpu_node,
                                         NULL);
}

static gboolean
is_remote_node (UfoNode *node, gpointer user_data)
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
                                         is_remote_node,
                                         NULL);
}

static void
unref_node_array (UfoNode **array,
                  guint n_nodes)
{
    for (guint i = 0; i < n_nodes; i++) {
        if (array[i] != NULL) {
            g_object_unref (array[i]);
            array[i] = NULL;
        }
    }
}

static void
ufo_arch_graph_dispose (GObject *object)
{
    UfoArchGraphPrivate *priv;

    priv = UFO_ARCH_GRAPH_GET_PRIVATE (object);

    unref_node_array (priv->cpu_nodes, priv->n_cpus);
    unref_node_array (priv->gpu_nodes, priv->n_gpus);
    unref_node_array (priv->remote_nodes, priv->n_remotes);

    if (priv->resources != NULL) {
        g_object_unref (priv->resources);
        priv->resources = NULL;
    }

    G_OBJECT_CLASS (ufo_arch_graph_parent_class)->dispose (object);
}

static void
ufo_arch_graph_finalize (GObject *object)
{
    UfoArchGraphPrivate *priv;

    priv = UFO_ARCH_GRAPH_GET_PRIVATE (object);

    g_free (priv->cpu_nodes);
    g_free (priv->gpu_nodes);
    g_free (priv->remote_nodes);

    priv->cpu_nodes = NULL;
    priv->gpu_nodes = NULL;
    priv->remote_nodes = NULL;

    G_OBJECT_CLASS (ufo_arch_graph_parent_class)->finalize (object);
}

static void
ufo_arch_graph_class_init (UfoArchGraphClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = ufo_arch_graph_dispose;
    oclass->finalize = ufo_arch_graph_finalize;

    g_type_class_add_private(klass, sizeof(UfoArchGraphPrivate));
}

static void
ufo_arch_graph_init (UfoArchGraph *self)
{
    UfoArchGraphPrivate *priv;
    self->priv = priv = UFO_ARCH_GRAPH_GET_PRIVATE (self);

    priv->cpu_nodes = NULL;
    priv->gpu_nodes = NULL;
    priv->remote_nodes = NULL;
}
