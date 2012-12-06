#ifndef UFO_TASK_IFACE_H
#define UFO_TASK_IFACE_H

#include <glib-object.h>
#include <ufo-buffer.h>
#include <ufo-resources.h>

G_BEGIN_DECLS

#define UFO_TYPE_TASK             (ufo_task_get_type())
#define UFO_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TASK, UfoTask))
#define UFO_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TASK, UfoTaskIface))
#define UFO_IS_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TASK))
#define UFO_IS_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TASK))
#define UFO_TASK_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_TASK, UfoTaskIface))

#define UFO_TASK_ERROR            ufo_task_error_quark()

typedef struct _UfoTask             UfoTask;
typedef struct _UfoTaskIface        UfoTaskIface;
typedef struct _UfoInputParameter   UfoInputParameter;

typedef enum {
    UFO_TASK_ERROR_SETUP
} UfoTaskError;

typedef enum {
    UFO_TASK_MODE_SINGLE,
    UFO_TASK_MODE_REDUCE
} UfoTaskMode;

struct _UfoTaskIface {
    GTypeInterface parent_iface;

    void (*setup)           (UfoTask        *task,
                             UfoResources   *resources,
                             GError        **error);
    void (*get_structure)   (UfoTask        *task,
                             guint          *n_inputs,
                             guint         **n_dims,
                             UfoTaskMode    *mode);
    void (*get_requisition) (UfoTask        *task,
                             UfoBuffer     **inputs,
                             UfoRequisition *requisition);
};

void   ufo_task_setup           (UfoTask          *task,
                                 UfoResources     *resources,
                                 GError          **error);
void   ufo_task_get_requisition (UfoTask          *task,
                                 UfoBuffer       **inputs,
                                 UfoRequisition   *requisition);
void   ufo_task_get_structure   (UfoTask          *task,
                                 guint            *n_inputs,
                                 guint           **n_dims,
                                 UfoTaskMode      *mode);

GQuark ufo_task_error_quark     (void);
GType  ufo_task_get_type        (void);

G_END_DECLS

#endif
