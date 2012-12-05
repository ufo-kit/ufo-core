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

/**
 * UfoInputParameter:
 * @n_dims: Number of dimension the input accept
 * @n_expected_items: Number of expected items. Use UFO_FILTER_INFINITE_INPUT to
 *      accept a data stream.
 *
 * Data structure for describing the parameters of an input as used by
 * ufo_filter_register_inputs().
 */
struct _UfoInputParameter {
    guint   n_dims;
    gint    n_expected_items;
};

struct _UfoTaskIface {
    GTypeInterface parent_iface;

    void (*setup)           (UfoTask        *task,
                             UfoResources   *resources,
                             GError        **error);
    void (*get_structure)   (UfoTask        *task,
                             guint          *n_inputs,
                             UfoInputParameter **in_params);
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
                                 UfoInputParameter
                                                 **in_params);

GQuark ufo_task_error_quark     (void);
GType  ufo_task_get_type        (void);

G_END_DECLS

#endif
