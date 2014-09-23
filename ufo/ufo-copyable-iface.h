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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COPYABLE_IFACE_H
#define COPYABLE_IFACE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_COPYABLE (ufo_copyable_get_type())
#define UFO_COPYABLE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_COPYABLE, UfoCopyable))
#define UFO_COPYABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_COPYABLE, UfoCopyableIface))
#define UFO_IS_COPYABLE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_COPYABLE))
#define UFO_IS_COPYABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_COPYABLE))
#define UFO_COPYABLE_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_COPYABLE, UfoCopyableIface))

typedef struct _UfoCopyable UfoCopyable;
typedef struct _UfoCopyableIface UfoCopyableIface;

struct _UfoCopyableIface {
    /*< private >*/
    GTypeInterface parent_iface;

    UfoCopyable* (*copy) (gpointer origin,
                          gpointer copy);
};

UfoCopyable * ufo_copyable_copy (gpointer origin,
                                 gpointer copy);

GType ufo_copyable_get_type (void);
G_END_DECLS
#endif
