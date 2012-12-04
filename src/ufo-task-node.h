#ifndef __UFO_TASK_NODE_H
#define __UFO_TASK_NODE_H

#include <glib-object.h>
#include <sched.h>
#include "ufo-node.h"
#include "ufo-group.h"

G_BEGIN_DECLS

#define UFO_TYPE_TASK_NODE             (ufo_task_node_get_type())
#define UFO_TASK_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TASK_NODE, UfoTaskNode))
#define UFO_IS_TASK_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TASK_NODE))
#define UFO_TASK_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TASK_NODE, UfoTaskNodeClass))
#define UFO_IS_TASK_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TASK_NODE))
#define UFO_TASK_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_TASK_NODE, UfoTaskNodeClass))

typedef struct _UfoTaskNode           UfoTaskNode;
typedef struct _UfoTaskNodeClass      UfoTaskNodeClass;
typedef struct _UfoTaskNodePrivate    UfoTaskNodePrivate;

typedef enum {
    UFO_SEND_BROADCAST,
    UFO_SEND_SCATTER
} UfoSendPattern;

/**
 * UfoTaskNode:
 *
 * Main object for organizing filters. The contents of the #UfoTaskNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoTaskNode {
    /*< private >*/
    UfoNode parent_instance;

    UfoTaskNodePrivate *priv;
};

/**
 * UfoTaskNodeClass:
 *
 * #UfoTaskNode class
 */
struct _UfoTaskNodeClass {
    /*< private >*/
    UfoNodeClass parent_class;
};

void            ufo_task_node_set_send_pattern      (UfoTaskNode    *task_node,
                                                     UfoSendPattern  pattern);
UfoSendPattern  ufo_task_node_get_send_pattern      (UfoTaskNode    *task_node);
void            ufo_task_node_set_out_group         (UfoTaskNode    *task_node,
                                                     UfoGroup       *group);
UfoGroup       *ufo_task_node_get_out_group         (UfoTaskNode    *task_node);
void            ufo_task_node_add_in_group          (UfoTaskNode    *task_node,
                                                     guint           pos,
                                                     UfoGroup       *group);
UfoGroup       *ufo_task_node_get_current_in_group  (UfoTaskNode    *task_node,
                                                     guint           pos);
void            ufo_task_node_switch_in_group       (UfoTaskNode    *task_node,
                                                     guint           pos);
void            ufo_task_node_set_proc_node         (UfoTaskNode    *task_node,
                                                     UfoNode        *proc_node);
UfoNode        *ufo_task_node_get_proc_node         (UfoTaskNode    *task_node);
GType           ufo_task_node_get_type              (void);

G_END_DECLS

#endif
