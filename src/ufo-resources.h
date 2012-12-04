#ifndef __UFO_RESOURCES_H
#define __UFO_RESOURCES_H

#include <glib-object.h>
#include "ufo-config.h"
#include "ufo-buffer.h"

G_BEGIN_DECLS

#define UFO_TYPE_RESOURCES             (ufo_resources_get_type())
#define UFO_RESOURCES(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RESOURCES, UfoResources))
#define UFO_IS_RESOURCES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RESOURCES))
#define UFO_RESOURCES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RESOURCES, UfoResourcesClass))
#define UFO_IS_RESOURCES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RESOURCES))
#define UFO_RESOURCES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RESOURCES, UfoResourcesClass))

typedef struct _UfoResources           UfoResources;
typedef struct _UfoResourcesClass      UfoResourcesClass;
typedef struct _UfoResourcesPrivate    UfoResourcesPrivate;

#define UFO_RESOURCES_ERROR ufo_resources_error_quark()
typedef enum {
    UFO_RESOURCES_ERROR_LOAD_PROGRAM,
    UFO_RESOURCES_ERROR_CREATE_PROGRAM,
    UFO_RESOURCES_ERROR_BUILD_PROGRAM,
    UFO_RESOURCES_ERROR_CREATE_KERNEL
} UfoResourcesError;


/**
 * opencl_map_error: (skip)
 */

/**
 * UFO_RESOURCES_CHECK_CLERR:
 * @error: OpenCL error code
 *
 * Check the return value of OpenCL functions and issue a warning with file and
 * line number if an error occured.
 */
#define UFO_RESOURCES_CHECK_CLERR(error) { \
    if ((error) != CL_SUCCESS) g_log("ocl", G_LOG_LEVEL_CRITICAL, "Error <%s:%i>: %s", __FILE__, __LINE__, ufo_resources_clerr((error))); }

/**
 * UfoResources:
 *
 * Manages OpenCL resources. The contents of the #UfoResources structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoResources {
    /*< private >*/
    GObject parent_instance;

    UfoResourcesPrivate *priv;
};

/**
 * UfoResourcesClass:
 *
 * #UfoResources class
 */
struct _UfoResourcesClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoResources   * ufo_resources_new                      (UfoConfig      *config);
gpointer         ufo_resources_get_kernel               (UfoResources   *resources,
                                                         const gchar    *filename,
                                                         const gchar    *kernel,
                                                         GError        **error);
gpointer         ufo_resources_get_kernel_from_source   (UfoResources   *resources,
                                                         const gchar    *source,
                                                         const gchar    *kernel,
                                                         GError        **error);
gpointer         ufo_resources_get_context              (UfoResources   *resources);
GList          * ufo_resources_get_cmd_queues           (UfoResources   *resources);
const gchar    * ufo_resources_clerr                    (int             error);
GType            ufo_resources_get_type                 (void);
GQuark           ufo_resources_error_quark              (void);

G_END_DECLS

#endif
