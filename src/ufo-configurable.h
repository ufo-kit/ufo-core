#ifndef __UFO_CONFIGURABLE_H
#define __UFO_CONFIGURABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_CONFIGURABLE             (ufo_configurable_get_type())
#define UFO_CONFIGURABLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CONFIGURABLE, UfoConfigurable))
#define UFO_CONFIGURABLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CONFIGURABLE, UfoConfigurableIface))
#define UFO_IS_CONFIGURABLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CONFIGURABLE))
#define UFO_IS_CONFIGURABLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CONFIGURABLE))
#define UFO_CONFIGURABLE_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_CONFIGURABLE, UfoConfigurableIface))

typedef struct _UfoConfigurable         UfoConfigurable;
typedef struct _UfoConfigurableIface    UfoConfigurableIface;

struct _UfoConfigurableIface {
    GTypeInterface parent_iface;
};

GType ufo_configurable_get_type (void);

G_END_DECLS

#endif
