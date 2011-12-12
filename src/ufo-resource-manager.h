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

const gchar* opencl_map_error(int error);

#define CHECK_ERROR(error) { \
    if ((error) != CL_SUCCESS) g_message("OpenCL error <%s:%i>: %s", __FILE__, __LINE__, opencl_map_error((error))); }

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

UfoResourceManager *ufo_resource_manager();
void ufo_resource_manager_add_paths(UfoResourceManager *self, const gchar *paths);
gboolean ufo_resource_manager_add_program(UfoResourceManager *self, const gchar *filename, const gchar *options, GError **error);
gpointer ufo_resource_manager_get_kernel(UfoResourceManager *self, const gchar *kernel, GError **error);
gpointer ufo_resource_manager_get_context(UfoResourceManager *self);
gpointer ufo_resource_manager_memdup(UfoResourceManager *manager, gpointer memobj);
gpointer ufo_resource_manager_memalloc(UfoResourceManager *manager, gsize size);
guint ufo_resource_manager_get_number_of_gpus(UfoResourceManager *resource_manager);
UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *resource_manager, UfoStructure structure, gint32 dimensions[4], float *data, gpointer command_queue);
/* UfoBuffer *ufo_resource_manager_copy_buffer(UfoResourceManager *self, UfoBuffer *buffer); */
void ufo_resource_manager_release_buffer(UfoResourceManager *self, UfoBuffer *buffer);
void ufo_resource_manager_get_command_queues(UfoResourceManager *resource_manager, gpointer *command_queues, int *num_queues);
guint ufo_resource_manager_get_new_id(UfoResourceManager *resource_manager);

GType ufo_resource_manager_get_type(void);

#endif
