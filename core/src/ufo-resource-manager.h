#ifndef __UFO_RESOURCE_MANAGER_H
#define __UFO_RESOURCE_MANAGER_H

#include <glib-object.h>
#include <CL/cl.h>

#include "ufo-buffer.h"

#define UFO_TYPE_RESOURCE_MANAGER             (ufo_resource_manager_get_type())
#define UFO_RESOURCE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManager))
#define UFO_IS_RESOURCE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RESOURCE_MANAGER))
#define UFO_RESOURCE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerClass))
#define UFO_IS_RESOURCE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RESOURCE_MANAGER))
#define UFO_RESOURCE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerClass))

typedef struct _UfoResourceManager           UfoResourceManager;
typedef struct _UfoResourceManagerClass      UfoResourceManagerClass;
typedef struct _UfoResourceManagerPrivate    UfoResourceManagerPrivate;


/**
 * \class UfoResourceManager
 * \brief Manages GPU and UfoBuffer resources
 *
 * <b>Signals</b>
 *
 * <b>Properties</b>
 */
struct _UfoResourceManager {
    GObject parent_instance;

    /* private */
    UfoResourceManagerPrivate *priv;
};

struct _UfoResourceManagerClass {
    GObjectClass parent_class;
};

UfoResourceManager *ufo_resource_manager_new();
gboolean ufo_resource_manager_add_program(UfoResourceManager *self, const gchar *filename, GError **error);
gpointer ufo_resource_manager_get_kernel(UfoResourceManager *self, const gchar *kernel, GError **error);
UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *self, guint32 width, guint32 height, float *data);
UfoBuffer *ufo_resource_manager_request_finish_buffer(UfoResourceManager *self);
void ufo_resource_manager_release_buffer(UfoResourceManager *self, UfoBuffer *buffer);

GType ufo_resource_manager_get_type(void);

#endif
