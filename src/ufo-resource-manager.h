#ifndef __UFO_RESOURCE_MANAGER_H
#define __UFO_RESOURCE_MANAGER_H

#include <glib-object.h>
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

const gchar *opencl_map_error(int error);

/**
 * CHECK_OPENCL_ERROR:
 * @error: OpenCL error code
 *
 * Check the return value of OpenCL functions and issue a warning with file and
 * line number if an error occured.
 */
#define CHECK_OPENCL_ERROR(error) { \
    if ((error) != CL_SUCCESS) g_warning("OpenCL error <%s:%i>: %s", __FILE__, __LINE__, opencl_map_error((error))); }

/**
 * UfoResourceManager:
 *
 * Manages OpenCL resources. The contents of the #UfoResourceManager structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoResourceManager {
    /*< private >*/
    GObject parent_instance;

    UfoResourceManagerPrivate *priv;
};

/**
 * UfoResourceManagerClass:
 *
 * #UfoResourceManager class
 */
struct _UfoResourceManagerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoResourceManager *ufo_resource_manager(void);

void ufo_resource_manager_add_paths(UfoResourceManager *manager, const gchar *paths);
gboolean ufo_resource_manager_add_program(UfoResourceManager *manager, const gchar *filename, const gchar *options, GError **error);
gpointer ufo_resource_manager_get_kernel(UfoResourceManager *manager, const gchar *kernel_name, GError **error);
gpointer ufo_resource_manager_get_context(UfoResourceManager *manager);
void ufo_resource_manager_get_command_queues(UfoResourceManager *manager, gpointer *command_queues, guint *num_queues);
guint ufo_resource_manager_get_number_of_devices(UfoResourceManager *manager);

gpointer ufo_resource_manager_memdup(UfoResourceManager *manager, gpointer memobj);
gpointer ufo_resource_manager_memalloc(UfoResourceManager *manager, gsize size);
UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *manager, guint num_dims, const guint *dim_size, gfloat *data, gpointer command_queue);
void ufo_resource_manager_release_buffer(UfoResourceManager *manager, UfoBuffer *buffer);
guint ufo_resource_manager_get_new_id(UfoResourceManager *manager);

GType ufo_resource_manager_get_type(void);
GQuark ufo_resource_manager_error_quark(void);

#endif
