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

#ifndef __UFO_CONFIGURABLE_H
#define __UFO_CONFIGURABLE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

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
    /*< private >*/
    GTypeInterface parent_iface;
};

GType ufo_configurable_get_type (void);

G_END_DECLS

#endif
