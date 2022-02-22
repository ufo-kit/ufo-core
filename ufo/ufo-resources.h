/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UFO_RESOURCES_H
#define __UFO_RESOURCES_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-buffer.h>

G_BEGIN_DECLS

#define UFO_TYPE_RESOURCES             (ufo_resources_get_type())
#define UFO_RESOURCES(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RESOURCES, UfoResources))
#define UFO_IS_RESOURCES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RESOURCES))
#define UFO_RESOURCES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RESOURCES, UfoResourcesClass))
#define UFO_IS_RESOURCES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RESOURCES))
#define UFO_RESOURCES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RESOURCES, UfoResourcesClass))

#define UFO_RESOURCES_ERROR             ufo_resources_error_quark()

typedef struct _UfoResources           UfoResources;
typedef struct _UfoResourcesClass      UfoResourcesClass;
typedef struct _UfoResourcesPrivate    UfoResourcesPrivate;


typedef enum {
    UFO_RESOURCES_ERROR_GENERAL,
    UFO_RESOURCES_ERROR_LOAD_PROGRAM,
    UFO_RESOURCES_ERROR_CREATE_PROGRAM,
    UFO_RESOURCES_ERROR_BUILD_PROGRAM,
    UFO_RESOURCES_ERROR_CREATE_KERNEL
} UfoResourcesError;

/**
 * UfoDeviceType:
 * @UFO_DEVICE_ALL: All devices
 * @UFO_DEVICE_CPU: Only CPU devices
 * @UFO_DEVICE_GPU: Only GPU devices
 * @UFO_DEVICE_ACC: Only accelerator devices such as Xeon Phi
 *
 * Types of OpenCL devices to query for. See UfoResources:"device-type".
 */
typedef enum {
    UFO_DEVICE_CPU = 1 << 0,
    UFO_DEVICE_GPU = 1 << 1,
    UFO_DEVICE_ACC = 1 << 2,
    UFO_DEVICE_ALL = UFO_DEVICE_CPU | UFO_DEVICE_GPU | UFO_DEVICE_ACC
} UfoDeviceType;

/**
 * UFO_RESOURCES_CHECK_CLERR:
 * @error: OpenCL error code
 *
 * Check the return value of OpenCL functions and issue a warning with file and
 * line number if an error occurred.
 */
#define UFO_RESOURCES_CHECK_CLERR(error) { \
    if ((error) != CL_SUCCESS) g_log("ocl", G_LOG_LEVEL_CRITICAL, "Error <%s:%i>: %s", __FILE__, __LINE__, ufo_resources_clerr((error))); }

/**
 * UFO_RESOURCES_CHECK_AND_SET:
 * @error: OpenCL error code
 * @g_error_loc: Return location for a GError or %NULL
 *
 * Check @error and set @g_error_loc accordingly.
 *
 * Returns: %TRUE if no error occurred, %FALSE otherwise.
 */
#define UFO_RESOURCES_CHECK_AND_SET(error, g_error_loc) { \
    cl_int tmp_error = error; \
    if ((tmp_error) != CL_SUCCESS) \
        g_set_error (g_error_loc, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_GENERAL, \
                     "OpenCL Error: %s", ufo_resources_clerr((tmp_error))); \
  }
/**
 * UFO_RESOURCES_CHECK_SET_AND_RETURN:
 * @error: OpenCL error code
 * @g_error_loc: Return location for a GError or %NULL
 *
 * Check @error and set @g_error_loc accordingly.
 *
 * Returns: %TRUE if no error occurred, %FALSE otherwise.
 */
#define UFO_RESOURCES_CHECK_SET_AND_RETURN(error, g_error_loc) { \
      cl_int tmp_error = error; \
      if (tmp_error != CL_SUCCESS) { \
        g_set_error (g_error_loc, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_GENERAL, \
                     "OpenCL Error <%s:%i>: %s", __FILE__, __LINE__, ufo_resources_clerr((tmp_error))); \
        return; \
        } \
    }

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

UfoResources   * ufo_resources_new                      (GError        **error);
void             ufo_resources_add_path                 (UfoResources   *resources,
                                                         const gchar    *path);
gpointer         ufo_resources_get_kernel               (UfoResources   *resources,
                                                         const gchar    *filename,
                                                         const gchar    *kernel,
                                                         const gchar    *options,
                                                         GError        **error);
gpointer         ufo_resources_get_kernel_from_source   (UfoResources   *resources,
                                                         const gchar    *source,
                                                         const gchar    *kernel,
                                                         const gchar    *options,
                                                         GError        **error);
gpointer         ufo_resources_get_cached_kernel        (UfoResources   *resources,
                                                         const gchar    *filename,
                                                         const gchar    *kernel,
                                                         GError        **error);
gchar          * ufo_resources_get_kernel_source        (UfoResources   *resources,
                                                         const gchar    *filename,
                                                         GError        **error);
gpointer         ufo_resources_get_context              (UfoResources   *resources);
unsigned int     ufo_resources_get_channel_order_2d     (UfoResources   *resources);
unsigned int     ufo_resources_get_channel_order_3d     (UfoResources   *resources);
GList          * ufo_resources_get_cmd_queues           (UfoResources   *resources);
GList          * ufo_resources_get_devices              (UfoResources   *resources);
GList          * ufo_resources_get_gpu_nodes            (UfoResources   *resources);
const gchar    * ufo_resources_clerr                    (int             error);
GType            ufo_resources_get_type                 (void);
GQuark           ufo_resources_error_quark              (void);

G_END_DECLS

#endif
