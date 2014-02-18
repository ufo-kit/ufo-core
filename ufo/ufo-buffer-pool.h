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

#ifndef __UFO_BUFFER_POOL_H
#define __UFO_BUFFER_POOL_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>
#include <ufo/ufo-buffer.h>


G_BEGIN_DECLS

#define UFO_TYPE_BUFFER_POOL             (ufo_buffer_pool_get_type())
#define UFO_BUFFER_POOL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_BUFFER_POOL, UfoBufferPool))
#define UFO_IS_BUFFER_POOL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BUFFER_POOL))
#define UFO_BUFFER_POOL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_BUFFER_POOL, UfoBufferPoolClass))
#define UFO_IS_BUFFER_POOL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BUFFER_POOL))
#define UFO_BUFFER_POOL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_BUFFER_POOL, UfoBufferPoolClass))


typedef struct _UfoBufferPool           UfoBufferPool;
typedef struct _UfoBufferPoolClass      UfoBufferPoolClass;
typedef struct _UfoBufferPoolPrivate    UfoBufferPoolPrivate;

/**
 * UfoBufferPool:
 *
 */
struct _UfoBufferPool {
    /*< private >*/
    GObject parent_instance;

    UfoBufferPoolPrivate *priv;
};

/**
 * UfoBufferPoolClass:
 *
 * #UfoBufferPool class
 */
struct _UfoBufferPoolClass {
    /*< private >*/
    GObjectClass parent_class;
};


UfoBufferPool  *ufo_buffer_pool_new             (gint capacity, gpointer context);
UfoBuffer      *ufo_buffer_pool_acquire         (UfoBufferPool *bp, UfoRequisition *req);
void            ufo_buffer_pool_release         (UfoBufferPool *bp, UfoBuffer *buffer);
GType           ufo_buffer_pool_get_type             (void);

G_END_DECLS

#endif
