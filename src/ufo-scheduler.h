#ifndef __UFO_SCHEDULER_H
#define __UFO_SCHEDULER_H

#include <glib-object.h>
#include "ufo-configuration.h"
#include "ufo-graph.h"
#include "ufo-resource-manager.h"
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_SCHEDULER             (ufo_scheduler_get_type())
#define UFO_SCHEDULER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_SCHEDULER, UfoScheduler))
#define UFO_IS_SCHEDULER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_SCHEDULER))
#define UFO_SCHEDULER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_SCHEDULER, UfoSchedulerClass))
#define UFO_IS_SCHEDULER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_SCHEDULER))
#define UFO_SCHEDULER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_SCHEDULER, UfoSchedulerClass))

typedef struct _UfoScheduler           UfoScheduler;
typedef struct _UfoSchedulerClass      UfoSchedulerClass;
typedef struct _UfoSchedulerPrivate    UfoSchedulerPrivate;

/**
 * UfoScheduler:
 *
 * The base class scheduler is responsible of assigning command queues to
 * filters (thus managing GPU device resources) and decide if to run a GPU or a
 * CPU. The actual schedule planning can be overriden.
 */
struct _UfoScheduler {
    /*< private >*/
    GObject parent_instance;

    UfoSchedulerPrivate *priv;
};

/**
 * UfoSchedulerClass:
 *
 * #UfoScheduler class
 */
struct _UfoSchedulerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoScheduler* ufo_scheduler_new          (UfoConfiguration   *config,
                                          UfoResourceManager *manager);
void          ufo_scheduler_run          (UfoScheduler       *scheduler,
                                          UfoGraph           *graph,
                                          GError**            error);
GType         ufo_scheduler_get_type     (void);

G_END_DECLS

#endif
