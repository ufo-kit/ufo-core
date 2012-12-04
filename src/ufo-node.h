#ifndef __UFO_NODE_H
#define __UFO_NODE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_NODE             (ufo_node_get_type())
#define UFO_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_NODE, UfoNode))
#define UFO_IS_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_NODE))
#define UFO_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_NODE, UfoNodeClass))
#define UFO_IS_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_NODE))
#define UFO_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_NODE, UfoNodeClass))

typedef struct _UfoNode           UfoNode;
typedef struct _UfoNodeClass      UfoNodeClass;
typedef struct _UfoNodePrivate    UfoNodePrivate;

/**
 * UfoNode:
 *
 * Main object for organizing filters. The contents of the #UfoNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoNode {
    /*< private >*/
    GObject parent_instance;

    UfoNodePrivate *priv;
};

/**
 * UfoNodeClass:
 *
 * #UfoNode class
 */
struct _UfoNodeClass {
    /*< private >*/
    GObjectClass parent_class;

    UfoNode * (*copy)   (UfoNode   *node,
                         GError   **error);
    gboolean  (*equal)  (UfoNode   *n1,
                         UfoNode   *n2);
};

UfoNode     *ufo_node_new       (gpointer    label);
gpointer     ufo_node_get_label (UfoNode    *node);
UfoNode     *ufo_node_copy      (UfoNode    *node,
                                 GError    **error);
gboolean     ufo_node_equal     (UfoNode    *n1,
                                 UfoNode    *n2);
GType        ufo_node_get_type  (void);

G_END_DECLS

#endif
