#define _GNU_SOURCE
#include <sched.h>
#include <ufo-cpu-node.h>

G_DEFINE_TYPE (UfoCpuNode, ufo_cpu_node, UFO_TYPE_NODE)

#define UFO_CPU_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CPU_NODE, UfoCpuNodePrivate))


struct _UfoCpuNodePrivate {
    cpu_set_t *mask;
};

UfoNode *
ufo_cpu_node_new (gpointer mask)
{
    UfoCpuNode *node;

    g_return_val_if_fail (mask != NULL, NULL);
    node = UFO_CPU_NODE (g_object_new (UFO_TYPE_CPU_NODE, NULL));
    node->priv->mask = g_memdup (mask, sizeof(cpu_set_t));
    return UFO_NODE (node);
}

gpointer
ufo_cpu_node_get_affinity (UfoCpuNode *cpu_node)
{
    g_return_val_if_fail (UFO_IS_CPU_NODE (cpu_node), NULL);
    return cpu_node->priv->mask;
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
    const gsize MAX_CPUS = MIN (16, CPU_SETSIZE);

    g_return_val_if_fail (UFO_IS_CPU_NODE (n1) && UFO_IS_CPU_NODE (n2), FALSE);
    priv1 = UFO_CPU_NODE_GET_PRIVATE (n1);
    priv2 = UFO_CPU_NODE_GET_PRIVATE (n2);

    for (gsize i = 0; i < MAX_CPUS; i++) {
        if (CPU_ISSET (i, priv1->mask) != CPU_ISSET (i, priv2->mask))
            return FALSE;
    }

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
