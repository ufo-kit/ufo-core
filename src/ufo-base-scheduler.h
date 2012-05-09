#ifndef __UFO_BASE_SCHEDULER_H
#define __UFO_BASE_SCHEDULER_H

#include <glib-object.h>
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_BASE_SCHEDULER             (ufo_base_scheduler_get_type())
#define UFO_BASE_SCHEDULER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseScheduler))
#define UFO_IS_BASE_SCHEDULER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BASE_SCHEDULER))
#define UFO_BASE_SCHEDULER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerClass))
#define UFO_IS_BASE_SCHEDULER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BASE_SCHEDULER))
#define UFO_BASE_SCHEDULER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerClass))

typedef struct _UfoBaseScheduler           UfoBaseScheduler;
typedef struct _UfoBaseSchedulerClass      UfoBaseSchedulerClass;
typedef struct _UfoBaseSchedulerPrivate    UfoBaseSchedulerPrivate;

/**
 * UfoBaseScheduler:
 *
 * The base class scheduler is responsible of assigning command queues to
 * filters (thus managing GPU device resources) and decide if to run a GPU or a
 * CPU. The actual schedule planning can be overriden.
 */
struct _UfoBaseScheduler {
    /*< private >*/
    GObject parent_instance;

    UfoBaseSchedulerPrivate *priv;
};

/**
 * UfoBaseSchedulerClass:
 *
 * #UfoBaseScheduler class
 */
struct _UfoBaseSchedulerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoBaseScheduler *ufo_base_scheduler_new(void);
void ufo_base_scheduler_add_filter(UfoBaseScheduler *scheduler, UfoFilter *filter);
void ufo_base_scheduler_run(UfoBaseScheduler *scheduler, GError **error);

GType ufo_base_scheduler_get_type(void);

G_END_DECLS

#endif
