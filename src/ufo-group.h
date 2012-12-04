#ifndef __UFO_GROUP_H
#define __UFO_GROUP_H

#include <glib-object.h>
#include "ufo-task-iface.h"
#include "ufo-buffer.h"

G_BEGIN_DECLS

#define UFO_TYPE_GROUP             (ufo_group_get_type())
#define UFO_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GROUP, UfoGroup))
#define UFO_IS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GROUP))
#define UFO_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GROUP, UfoGroupClass))
#define UFO_IS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GROUP))
#define UFO_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GROUP, UfoGroupClass))

typedef struct _UfoGroup           UfoGroup;
typedef struct _UfoGroupClass      UfoGroupClass;
typedef struct _UfoGroupPrivate    UfoGroupPrivate;

#define UFO_END_OF_STREAM (GINT_TO_POINTER(1))

/**
 * UfoGroup:
 *
 * Main object for organizing filters. The contents of the #UfoGroup structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoGroup {
    /*< private >*/
    GObject parent_instance;

    UfoGroupPrivate *priv;
};

/**
 * UfoGroupClass:
 *
 * #UfoGroup class
 */
struct _UfoGroupClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoGroup  * ufo_group_new                   (GList          *targets,
                                             gpointer        context);
UfoBuffer * ufo_group_pop_output_buffer     (UfoGroup       *group,
                                             UfoRequisition *requisition);
void        ufo_group_push_output_buffer    (UfoGroup       *group,
                                             UfoBuffer      *buffer);
UfoBuffer * ufo_group_pop_input_buffer      (UfoGroup       *group,
                                             UfoTask        *target);
void        ufo_group_push_input_buffer     (UfoGroup       *group,
                                             UfoTask        *target,
                                             UfoBuffer      *input);
void        ufo_group_finish                (UfoGroup       *group);
GType       ufo_group_get_type              (void);

G_END_DECLS

#endif
