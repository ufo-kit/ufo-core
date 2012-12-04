#ifndef __UFO_GPU_NODE_H
#define __UFO_GPU_NODE_H

#include <glib-object.h>
#include "ufo-node.h"

G_BEGIN_DECLS

#define UFO_TYPE_GPU_NODE             (ufo_gpu_node_get_type())
#define UFO_GPU_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GPU_NODE, UfoGpuNode))
#define UFO_IS_GPU_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GPU_NODE))
#define UFO_GPU_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GPU_NODE, UfoGpuNodeClass))
#define UFO_IS_GPU_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GPU_NODE))
#define UFO_GPU_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GPU_NODE, UfoGpuNodeClass))

typedef struct _UfoGpuNode           UfoGpuNode;
typedef struct _UfoGpuNodeClass      UfoGpuNodeClass;
typedef struct _UfoGpuNodePrivate    UfoGpuNodePrivate;

/**
 * UfoGpuNode:
 *
 * Main object for organizing filters. The contents of the #UfoGpuNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoGpuNode {
    /*< private >*/
    UfoNode parent_instance;

    UfoGpuNodePrivate *priv;
};

/**
 * UfoGpuNodeClass:
 *
 * #UfoGpuNode class
 */
struct _UfoGpuNodeClass {
    /*< private >*/
    UfoNodeClass parent_class;
};

UfoNode  *ufo_gpu_node_new              (gpointer    cmd_queue);
gpointer  ufo_gpu_node_get_cmd_queue    (UfoGpuNode *gpu_node);
GType     ufo_gpu_node_get_type         (void);

G_END_DECLS

#endif
