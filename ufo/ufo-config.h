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

#ifndef __UFO_CONFIG_H
#define __UFO_CONFIG_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_CONFIG             (ufo_config_get_type())
#define UFO_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CONFIG, UfoConfig))
#define UFO_IS_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CONFIG))
#define UFO_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CONFIG, UfoConfigClass))
#define UFO_IS_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CONFIG))
#define UFO_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_CONFIG, UfoConfigClass))

typedef struct _UfoConfig           UfoConfig;
typedef struct _UfoConfigClass      UfoConfigClass;
typedef struct _UfoConfigPrivate    UfoConfigPrivate;

/**
 * UfoConfig:
 *
 * A #UfoConfig provides access to run-time specific settings.
 */
struct _UfoConfig {
    /*< private >*/
    GObject parent_instance;

    UfoConfigPrivate *priv;
};

/**
 * UfoConfigClass:
 *
 * #UfoConfig class
 */
struct _UfoConfigClass {
    /*< private >*/
    GObjectClass parent_class;
};

typedef enum {
    UFO_DEVICE_CPU = 1 << 0,
    UFO_DEVICE_GPU = 1 << 1,
    UFO_DEVICE_ALL = (1 << 1) | (1 << 0)
} UfoDeviceType;


UfoConfig   * ufo_config_new                (void);
void          ufo_config_add_paths          (UfoConfig *config,
                                             GList     *paths);
GList       * ufo_config_get_paths          (UfoConfig *config);
UfoDeviceType ufo_config_get_device_type    (UfoConfig *config);
GType         ufo_config_get_type           (void);

G_END_DECLS

#endif
