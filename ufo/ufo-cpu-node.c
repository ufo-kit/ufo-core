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
#include <sched.h>
#include <ufo/ufo-cpu-node.h>

G_DEFINE_TYPE (UfoCpuNode, ufo_cpu_node, UFO_TYPE_NODE)

#define UFO_CPU_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CPU_NODE, UfoCpuNodePrivate))


struct _UfoCpuNodePrivate {
#ifdef __APPLE__
    gpointer mask;
#else
    cpu_set_t *mask;
#endif
};

UfoNode *
ufo_cpu_node_new (gpointer mask)
{
    UfoCpuNode *node;

    g_return_val_if_fail (mask != NULL, NULL);
    node = UFO_CPU_NODE (g_object_new (UFO_TYPE_CPU_NODE, NULL));
#ifdef __APPLE__
    node->priv->mask = g_memdup (mask, sizeof (gpointer));
#else
    node->priv->mask = g_memdup (mask, sizeof (cpu_set_t));
#endif
    return UFO_NODE (node);
}

/**
 * ufo_cpu_node_get_affinity:
 * @node: A #UfoCpuNode
 *
 * Get affinity mask of @node.
 *
 * Returns: (transfer none): A pointer to the cpu_set_t mask associated with
 * @node.
 */
gpointer
ufo_cpu_node_get_affinity (UfoCpuNode *node)
{
    g_return_val_if_fail (UFO_IS_CPU_NODE (node), NULL);
    return node->priv->mask;
}

static void
ufo_cpu_node_finalize (GObject *object)
{
    UfoCpuNodePrivate *priv;

    priv = UFO_CPU_NODE_GET_PRIVATE (object);

    if (priv->mask) {
        g_free (priv->mask);
        priv->mask = NULL;
    }

    G_OBJECT_CLASS (ufo_cpu_node_parent_class)->finalize (object);
}

static UfoNode *
ufo_cpu_node_copy_real (UfoNode *node,
                        GError **error)
{
    return UFO_NODE (ufo_cpu_node_new (UFO_CPU_NODE (node)->priv->mask));
}

static gboolean
ufo_cpu_node_equal_real (UfoNode *n1,
                         UfoNode *n2)
{
    UfoCpuNodePrivate *priv1;
    UfoCpuNodePrivate *priv2;

#ifndef __APPLE__
    const gsize MAX_CPUS = MIN (16, CPU_SETSIZE);
#else
    const gsize MAX_CPUS = 16;
#endif

    g_return_val_if_fail (UFO_IS_CPU_NODE (n1) && UFO_IS_CPU_NODE (n2), FALSE);
    priv1 = UFO_CPU_NODE_GET_PRIVATE (n1);
    priv2 = UFO_CPU_NODE_GET_PRIVATE (n2);

#ifndef __APPLE__
    for (gsize i = 0; i < MAX_CPUS; i++) {
        if (CPU_ISSET (i, priv1->mask) != CPU_ISSET (i, priv2->mask))
            return FALSE;
    }
#endif

    return TRUE;
}

static void
ufo_cpu_node_class_init (UfoCpuNodeClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class = UFO_NODE_CLASS (klass);

    object_class->finalize = ufo_cpu_node_finalize;
    node_class->copy = ufo_cpu_node_copy_real;
    node_class->equal = ufo_cpu_node_equal_real;

    g_type_class_add_private(klass, sizeof(UfoCpuNodePrivate));
}

static void
ufo_cpu_node_init (UfoCpuNode *self)
{
    UfoCpuNodePrivate *priv;
    self->priv = priv = UFO_CPU_NODE_GET_PRIVATE (self);
    priv->mask = NULL;
}
