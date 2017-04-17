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

#ifndef __UFO_BUFFER_H
#define __UFO_BUFFER_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_BUFFER             (ufo_buffer_get_type())
#define UFO_BUFFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_BUFFER, UfoBuffer))
#define UFO_IS_BUFFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BUFFER))
#define UFO_BUFFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_BUFFER, UfoBufferClass))
#define UFO_IS_BUFFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BUFFER))
#define UFO_BUFFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_BUFFER, UfoBufferClass))

#define UFO_TYPE_PARAM_BUFFER       (ufo_buffer_param_get_type())
#define UFO_IS_PARAM_SPEC_BUFFER(pspec)  (G_TYPE_CHECK_INSTANCE_TYPE((pspec), UFO_TYPE_PARAM_BUFFER))
#define UFO_BUFFER_PARAM_SPEC(pspec)     (G_TYPE_CHECK_INSTANCE_CAST((pspec), UFO_TYPE_PARAM_BUFFER, UfoBufferParamSpec))

typedef struct _UfoBuffer           UfoBuffer;
typedef struct _UfoBufferClass      UfoBufferClass;
typedef struct _UfoBufferPrivate    UfoBufferPrivate;
typedef struct _UfoBufferParamSpec  UfoBufferParamSpec;
typedef struct _UfoRequisition      UfoRequisition;
typedef struct _UfoRegion           UfoRegion;

/**
 * UfoBuffer:
 *
 * Represents n-dimensional data. The contents of the #UfoBuffer structure are
 * private and should only be accessed via the provided API.
 */
struct _UfoBuffer {
    /*< private >*/
    GObject parent_instance;

    UfoBufferPrivate *priv;
};

#define UFO_BUFFER_MAX_NDIMS 3

/**
 * UfoBufferClass:
 *
 * #UfoBuffer class
 */
struct _UfoBufferClass {
    /*< private >*/
    GObjectClass parent_class;
};

/**
 * UfoBufferParamSpec:
 *
 * UfoBufferParamSpec class
 */
struct _UfoBufferParamSpec {
    /*< private >*/
    GParamSpec  parent_instance;

    UfoBuffer   *default_value;
};

struct _UfoRequisition {
    guint n_dims;
    gsize dims[UFO_BUFFER_MAX_NDIMS];
};

struct _UfoRegion {
    gsize origin[UFO_BUFFER_MAX_NDIMS];
    gsize size[UFO_BUFFER_MAX_NDIMS];
};

typedef enum {
    UFO_BUFFER_DEPTH_INVALID,
    UFO_BUFFER_DEPTH_8U,
    UFO_BUFFER_DEPTH_16U,
    UFO_BUFFER_DEPTH_16S,
    UFO_BUFFER_DEPTH_32S,
    UFO_BUFFER_DEPTH_32U,
    UFO_BUFFER_DEPTH_32F
} UfoBufferDepth;

typedef enum {
    UFO_BUFFER_LOCATION_HOST = 0,
    UFO_BUFFER_LOCATION_DEVICE,
    UFO_BUFFER_LOCATION_DEVICE_IMAGE,
    UFO_BUFFER_LOCATION_INVALID,
    UFO_BUFFER_LOCATION_DEVICE_DIRECT_GMA
} UfoBufferLocation;

UfoBuffer*  ufo_buffer_new                  (UfoRequisition *requisition,
                                             gpointer        context);
UfoBuffer*  ufo_buffer_new_with_size        (GList          *dims,
                                             gpointer        context);
UfoBuffer*  ufo_buffer_new_with_data        (UfoRequisition *requisition,
                                             gpointer        data,
                                             gpointer        context);
void        ufo_buffer_resize               (UfoBuffer      *buffer,
                                             UfoRequisition *requisition);
gint        ufo_buffer_cmp_dimensions       (UfoBuffer      *buffer,
                                             UfoRequisition *requisition);
void        ufo_buffer_get_requisition      (UfoBuffer      *buffer,
                                             UfoRequisition *requisition);
gsize       ufo_buffer_get_size             (UfoBuffer      *buffer);
void        ufo_buffer_copy                 (UfoBuffer      *src,
                                             UfoBuffer      *dst);
UfoBuffer  *ufo_buffer_dup                  (UfoBuffer      *buffer);
void        ufo_buffer_set_host_array       (UfoBuffer      *buffer,
                                             gpointer        array,
                                             gboolean        free_data);
void        ufo_buffer_copy_host_array      (UfoBuffer      *buffer,
		                                     gpointer        array);
gfloat*     ufo_buffer_get_host_array       (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);
void        ufo_buffer_set_device_array     (UfoBuffer      *buffer,
                                             gpointer        array,
                                             gboolean        free_data);
gpointer    ufo_buffer_get_device_array     (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);
gpointer    ufo_buffer_get_device_array_view(UfoBuffer      *buffer,
                                             gpointer        cmd_queue,
                                             UfoRegion      *region);
gpointer    ufo_buffer_get_device_image     (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);
gpointer    ufo_buffer_get_device_array_with_offset
                                            (UfoBuffer      *buffer,
                                             gpointer        cmd_queue,
                                             gsize           offset);
UfoBufferLocation
            ufo_buffer_get_location         (UfoBuffer      *buffer);
void        ufo_buffer_discard_location     (UfoBuffer      *buffer);
void        ufo_buffer_convert              (UfoBuffer      *buffer,
                                             UfoBufferDepth  depth);
void        ufo_buffer_convert_from_data    (UfoBuffer      *buffer,
                                             gconstpointer   data,
                                             UfoBufferDepth  depth);
GValue     *ufo_buffer_get_metadata         (UfoBuffer      *buffer,
                                             const gchar    *name);
void        ufo_buffer_set_metadata         (UfoBuffer      *buffer,
                                             const gchar    *name,
                                             GValue   *value);
void        ufo_buffer_copy_metadata        (UfoBuffer      *src,
                                             UfoBuffer      *dst);
GList      *ufo_buffer_get_metadata_keys    (UfoBuffer      *buffer);

gfloat      ufo_buffer_max                  (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);
gfloat      ufo_buffer_min                  (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);

GType       ufo_buffer_get_type             (void);

GParamSpec* ufo_buffer_param_spec           (const gchar*   name,
                                             const gchar*   nick,
                                             const gchar*   blurb,
                                             UfoBuffer*     default_value,
                                             GParamFlags    flags);
GType       ufo_buffer_param_get_type       (void);

G_END_DECLS

#endif
