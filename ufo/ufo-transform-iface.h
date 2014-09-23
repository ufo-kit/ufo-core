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

#ifndef UFO_TRANSFORM_IFACE_H
#define UFO_TRANSFORM_IFACE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>
#include <ufo/ufo-buffer.h>

G_BEGIN_DECLS

#define UFO_TYPE_TRANSFORM (ufo_transform_get_type())
#define UFO_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TRANSFORM, UfoTransform))
#define UFO_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TRANSFORM, UfoTransformIface))
#define UFO_IS_TRANSFORM(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TRANSFORM))
#define UFO_IS_TRANSFORM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TRANSFORM))
#define UFO_TRANSFORM_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_TRANSFORM, UfoTransformIface))

typedef struct _UfoTransform UfoTransform;
typedef struct _UfoTransformIface UfoTransformIface;

struct _UfoTransformIface {
    /*< private >*/
    GTypeInterface parent_iface;

    gboolean (*direct) (UfoTransform *transform,
                        UfoBuffer    *input,
                        UfoBuffer    *output,
                        gpointer     pevent);

    gboolean (*inverse) (UfoTransform *transform,
                         UfoBuffer    *input,
                         UfoBuffer    *output,
                         gpointer     pevent);
};

gboolean
ufo_transform_direct (UfoTransform *transform,
                      UfoBuffer    *input,
                      UfoBuffer    *output,
                      gpointer     pevent);
gboolean
ufo_transform_inverse (UfoTransform *transform,
                       UfoBuffer    *input,
                       UfoBuffer    *output,
                       gpointer     pevent);

GType ufo_transform_get_type (void);

G_END_DECLS
#endif
