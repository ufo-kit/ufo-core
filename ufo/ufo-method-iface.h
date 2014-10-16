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

#ifndef UFO_METHOD_IFACE_H
#define UFO_METHOD_IFACE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>
#include <ufo/ufo-buffer.h>

G_BEGIN_DECLS

#define UFO_TYPE_METHOD (ufo_method_get_type())
#define UFO_METHOD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_METHOD, UfoMethod))
#define UFO_METHOD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_METHOD, UfoMethodIface))
#define UFO_IS_METHOD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_METHOD))
#define UFO_IS_METHOD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_METHOD))
#define UFO_METHOD_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_METHOD, UfoMethodIface))

typedef struct _UfoMethod UfoMethod;
typedef struct _UfoMethodIface UfoMethodIface;

struct _UfoMethodIface {
    /*< private >*/
    GTypeInterface parent_iface;

    gboolean (*process) (UfoMethod *method,
                         UfoBuffer *input,
                         UfoBuffer *output,
                         gpointer  pevent);
};

gboolean
ufo_method_process (UfoMethod *method,
                    UfoBuffer *input,
                    UfoBuffer *output,
                    gpointer  pevent);

GType ufo_method_get_type (void);

G_END_DECLS
#endif
