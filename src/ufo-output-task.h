#ifndef __UFO_OUTPUT_TASK_H
#define __UFO_OUTPUT_TASK_H

#include <glib-object.h>
#include "ufo-task-node.h"

G_BEGIN_DECLS

#define UFO_TYPE_OUTPUT_TASK             (ufo_output_task_get_type())
#define UFO_OUTPUT_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_OUTPUT_TASK, UfoOutputTask))
#define UFO_IS_OUTPUT_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_OUTPUT_TASK))
#define UFO_OUTPUT_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_OUTPUT_TASK, UfoOutputTaskClass))
#define UFO_IS_OUTPUT_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_OUTPUT_TASK))
#define UFO_OUTPUT_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_OUTPUT_TASK, UfoOutputTaskClass))

typedef struct _UfoOutputTask           UfoOutputTask;
typedef struct _UfoOutputTaskClass      UfoOutputTaskClass;
typedef struct _UfoOutputTaskPrivate    UfoOutputTaskPrivate;

/**
 * UfoOutputTask:
 *
 * Main object for organizing filters. The contents of the #UfoOutputTask structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoOutputTask {
    /*< private >*/
    UfoTaskNode parent_instance;

    UfoOutputTaskPrivate *priv;
};

/**
 * UfoOutputTaskClass:
 *
 * #UfoOutputTask class
 */
struct _UfoOutputTaskClass {
    /*< private >*/
    UfoTaskNodeClass parent_class;
};

UfoNode   * ufo_output_task_new                     (guint n_dims);
void        ufo_output_task_get_output_requisition  (UfoOutputTask *task,
                                                     UfoRequisition *requisition);
UfoBuffer * ufo_output_task_get_output_buffer       (UfoOutputTask *task);
void        ufo_output_task_release_output_buffer   (UfoOutputTask *task,
                                                     UfoBuffer *buffer);
GType       ufo_output_task_get_type                (void);

G_END_DECLS

#endif
