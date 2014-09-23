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

#ifndef __UFO_PROCESSOR_H
#define __UFO_PROCESSOR_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>
#include <ufo/ufo-resources.h>

G_BEGIN_DECLS

#define UFO_TYPE_PROCESSOR              (ufo_processor_get_type())
#define UFO_PROCESSOR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_PROCESSOR, UfoProcessor))
#define UFO_IS_PROCESSOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_PROCESSOR))
#define UFO_PROCESSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_PROCESSOR, UfoProcessorClass))
#define UFO_IS_PROCESSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_PROCESSOR)
#define UFO_PROCESSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_PROCESSOR, UfoProcessorClass))

typedef struct _UfoProcessor         UfoProcessor;
typedef struct _UfoProcessorClass    UfoProcessorClass;
typedef struct _UfoProcessorPrivate  UfoProcessorPrivate;

/**
 * UfoProcessor:
 *
 * Describes a basic processing element that is used inside a filter
 *
 */
struct _UfoProcessor {
    /*< private >*/
    GObject parent_instance;
    UfoProcessorPrivate *priv;
};

/**
 * UfoProcessorClass:
 *
 * #UfoProcessor class
 */
struct _UfoProcessorClass {
    /*< private >*/
    GObjectClass parent_class;

    void (*setup) (UfoProcessor *processor,
                   UfoResources *resources,
                   GError       **error);

    void (*configure) (UfoProcessor *processor);
};

UfoProcessor*       ufo_processor_new       (void);
void                ufo_processor_setup     (UfoProcessor *processor,
                                             UfoResources *resources,
                                             GError       **error);
void                ufo_processor_configure (UfoProcessor *processor);
GType               ufo_processor_get_type  (void);

G_END_DECLS
#endif
