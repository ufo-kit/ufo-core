#ifndef __UFO_CPU_NODE_H
#define __UFO_CPU_NODE_H

#include <glib-object.h>
#include <ufo-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_CPU_NODE             (ufo_cpu_node_get_type())
#define UFO_CPU_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CPU_NODE, UfoCpuNode))
#define UFO_IS_CPU_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CPU_NODE))
#define UFO_CPU_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CPU_NODE, UfoCpuNodeClass))
#define UFO_IS_CPU_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CPU_NODE))
#define UFO_CPU_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_CPU_NODE, UfoCpuNodeClass))

typedef struct _UfoCpuNode           UfoCpuNode;
typedef struct _UfoCpuNodeClass      UfoCpuNodeClass;
typedef struct _UfoCpuNodePrivate    UfoCpuNodePrivate;

/**
 * UfoCpuNode:
 *
 * Main object for organizing filters. The contents of the #UfoCpuNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoCpuNode {
    /*< private >*/
    UfoNode parent_instance;

    UfoCpuNodePrivate *priv;
};

/**
 * UfoCpuNodeClass:
 *
 * #UfoCpuNode class
 */
struct _UfoCpuNodeClass {
    /*< private >*/
    UfoNodeClass parent_class;
};

UfoNode     *ufo_cpu_node_new           (gpointer mask);
gpointer     ufo_cpu_node_get_affinity  (UfoCpuNode *cpu_node);
GType        ufo_cpu_node_get_type      (void);

G_END_DECLS

#endif
