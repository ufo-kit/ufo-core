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
 * @Short_description: Describe relationship between hardware resources
 * @Title: UfoArchGraph
 */

G_DEFINE_TYPE (UfoArchGraph, ufo_arch_graph, UFO_TYPE_GRAPH)

#define UFO_ARCH_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ARCH_GRAPH, UfoArchGraphPrivate))

struct _UfoArchGraphPrivate {
    UfoResources *resources;
    GList *remotes;
    UfoNode **cpu_nodes;
    UfoNode **gpu_nodes;
    UfoNode **remote_nodes;
    guint n_cpus;
    guint n_gpus;
    guint n_remotes;
};

enum {
    PROP_0,
    PROP_RESOURCES,
    PROP_REMOTES,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ufo_arch_graph_new:
 * @resources: (allow-none): An initialized #UfoResources object or %NULL
 * @remotes: (element-type utf8): (allow-none): A #GList containing
 *      address strings.
 *
 * Returns: A new #UfoArchGraph.
 */
UfoGraph *
ufo_arch_graph_new (UfoResources *resources,
                    GList *remotes)
{
    return UFO_GRAPH (g_object_new (UFO_TYPE_ARCH_GRAPH,
			    "resources", resources,
			    "remotes", remotes,
			    NULL));
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

/**
 * ufo_arch_graph_get_resources:
 * @graph: A #UfoArchGraph object
 *
 * Get the resources of @graph.
 *
 * Returns: (transfer none): the associated #UfoResources object.
 */
UfoResources *
ufo_arch_graph_get_resources (UfoArchGraph *graph)
{
    g_return_val_if_fail (UFO_IS_ARCH_GRAPH (graph), NULL);
    return graph->priv->resources;
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
    return ufo_graph_get_nodes_filtered (UFO_GRAPH (graph), is_gpu_node, NULL);
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
    return ufo_graph_get_nodes_filtered (UFO_GRAPH (graph), is_remote_node, NULL);
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
ufo_arch_graph_constructed (GObject *object)
{
    UfoGraph *graph;
    UfoArchGraphPrivate *priv;
    GList *cmd_queues;
    cpu_set_t mask;

    graph = UFO_GRAPH (object);
    priv = UFO_ARCH_GRAPH_GET_PRIVATE (object);

    /* Create CPU nodes */
    priv->n_cpus = (guint) get_nprocs ();
    priv->cpu_nodes = g_new0 (UfoNode *, priv->n_cpus);

    for (guint i = 0; i < priv->n_cpus; i++) {
        CPU_ZERO (&mask);
        CPU_SET (i, &mask);
        priv->cpu_nodes[i] = ufo_cpu_node_new (&mask);
    }

    if (priv->resources == NULL) {
        GError *error = NULL;

        priv->resources = ufo_resources_new (&error);

        if (error != NULL) {
            g_error ("Could not initialize resources: %s", error->message);
            g_error_free (error);
        }
    }

    /* Create GPU nodes, each one is associated with its own command queue. */
    cmd_queues = ufo_resources_get_cmd_queues (priv->resources);

    priv->n_gpus = g_list_length (cmd_queues);
    priv->gpu_nodes = g_new0 (UfoNode *, priv->n_gpus);

    for (guint i = 0; i < priv->n_gpus; i++) {
        priv->gpu_nodes[i] = ufo_gpu_node_new (g_list_nth_data (cmd_queues, i));
        g_debug ("Create new UfoGpuNode-%p", (gpointer) priv->gpu_nodes[i]);
    }

    /* Create remote nodes */
    priv->n_remotes = g_list_length (priv->remotes);

    if (priv->n_remotes > 0) {
        priv->remote_nodes = g_new0 (UfoNode *, priv->n_remotes);

        for (guint i = 0; i < priv->n_remotes; i++) {
            priv->remote_nodes[i] = ufo_remote_node_new ((gchar *) g_list_nth_data (priv->remotes, i));
        }
    }

    /*
     * Connect all CPUs to all GPUs. In the future this is the place for a
     * NUMA-specific mapping.
     */
    for (guint i = 0; i < priv->n_cpus; i++) {
        for (guint j = 0; j < priv->n_gpus; j++) {
            ufo_graph_connect_nodes (graph, priv->cpu_nodes[i], priv->gpu_nodes[j], NULL);
        }

        for (guint j = 0; j < priv->n_remotes; j++) {
            ufo_graph_connect_nodes (graph, priv->cpu_nodes[i], priv->remote_nodes[j], NULL);
        }
    }

    g_list_free (cmd_queues);

    G_OBJECT_CLASS (ufo_arch_graph_parent_class)->constructed (object);
}

static void
ufo_arch_graph_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    UfoArchGraphPrivate *priv;

    priv = UFO_ARCH_GRAPH_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_RESOURCES:
            {
                UfoResources *resources;

                g_assert (priv->resources == NULL);
                resources = g_value_get_object (value);

                if (resources)
                    priv->resources = g_object_ref (resources);
            }
            break;

        case PROP_REMOTES:
            {
                GValueArray *array;

                g_assert (priv->remotes == NULL);
                array = g_value_get_boxed (value);

                if (array != NULL) {
                    g_list_free (priv->remotes);

                    for (guint i = 0; i < array->n_values; i++) {
                        priv->remotes = g_list_append (priv->remotes,
                                                       g_strdup (g_value_get_string (&array->values[i])));
                    }
                }
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_arch_graph_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    UfoArchGraphPrivate *priv;

    priv = UFO_ARCH_GRAPH_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_RESOURCES:
            g_value_set_object (value, priv->resources);
            break;

        case PROP_REMOTES:
            g_value_set_boxed (value, priv->remotes);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
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
    g_list_free_full (priv->remotes, g_free);

    priv->cpu_nodes = NULL;
    priv->gpu_nodes = NULL;
    priv->remotes = NULL;
    priv->remote_nodes = NULL;

    G_OBJECT_CLASS (ufo_arch_graph_parent_class)->finalize (object);
}

static void
ufo_arch_graph_class_init (UfoArchGraphClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->constructed = ufo_arch_graph_constructed;
    oclass->dispose = ufo_arch_graph_dispose;
    oclass->finalize = ufo_arch_graph_finalize;
    oclass->set_property = ufo_arch_graph_set_property;
    oclass->get_property = ufo_arch_graph_get_property;

    properties[PROP_RESOURCES] =
        g_param_spec_object ("resources",
                             "A UfoResources object",
                             "A UfoResources object",
                             UFO_TYPE_RESOURCES,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_REMOTES] =
        g_param_spec_value_array ("remotes",
                                  "List with remote addresses",
                                  "List with remote addresses",
                                  g_param_spec_string ("remote",
                                                       "Remote address",
                                                       "Remote address",
                                                       "tcp://127.0.0.1:5554",
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE),
                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_property (oclass, PROP_RESOURCES, properties[PROP_RESOURCES]);
    g_object_class_install_property (oclass, PROP_REMOTES, properties[PROP_REMOTES]);

    g_type_class_add_private(klass, sizeof(UfoArchGraphPrivate));
}

static void
ufo_arch_graph_init (UfoArchGraph *self)
{
    UfoArchGraphPrivate *priv;
    self->priv = priv = UFO_ARCH_GRAPH_GET_PRIVATE (self);

    priv->cpu_nodes = NULL;
    priv->gpu_nodes = NULL;
    priv->remotes = NULL;
    priv->remote_nodes = NULL;
    priv->resources = NULL;
}
