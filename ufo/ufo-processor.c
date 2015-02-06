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

#include "ufo-processor.h"
#include "ufo-copyable-iface.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

static void ufo_copyable_interface_init (UfoCopyableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoProcessor, ufo_processor, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_COPYABLE,
                                                ufo_copyable_interface_init))

#define UFO_PROCESSOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PROCESSOR, UfoProcessorPrivate))

struct _UfoProcessorPrivate {
    UfoResources *resources;
    UfoProfiler  *profiler;
    gpointer      cmd_queue;
};

UfoProcessor *
ufo_processor_new (void)
{
    return (UfoProcessor *) g_object_new (UFO_TYPE_PROCESSOR,
                                          NULL);
}

void
ufo_processor_set_resources (UfoProcessor *processor,
                             UfoResources *resources)
{
    if (resources == NULL)
        return;

    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (processor);
    if (priv->resources)
        g_object_unref (priv->resources);
  
    priv->resources = g_object_ref (resources);
}
/**
 * ufo_processor_get_resources:
 * @processor: A #UfoProcessor 
 *
 * Returns: #UfoResources or %NULL if resources were not set before.
 */
UfoResources *
ufo_processor_get_resources (UfoProcessor *processor)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (processor);
    return priv->resources;
}

void
ufo_processor_set_profiler (UfoProcessor *processor,
                            UfoProfiler  *profiler)
{
    if (profiler == NULL)
        return;

    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (processor);
    if (priv->profiler)
        g_object_unref (priv->profiler);
  
    priv->profiler = g_object_ref (profiler);
}

/**
 * ufo_processor_get_profiler:
 * @processor: A #UfoProcessor 
 *
 * Returns: #UfoProfiler or %NULL if profiler was not set before.
 */
UfoProfiler *
ufo_processor_get_profiler (UfoProcessor *processor)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (processor);
    return priv->profiler;
}

void
ufo_processor_set_command_queue (UfoProcessor *processor,
                                 gpointer cmd_queue)
{
    if (cmd_queue == NULL)
        return;
    
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (processor); 
    if (priv->cmd_queue) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseCommandQueue (priv->cmd_queue));
    }

    priv->cmd_queue = cmd_queue;
    UFO_RESOURCES_CHECK_CLERR (clRetainCommandQueue (priv->cmd_queue));
}

/**
 * ufo_processor_get_command_queue:
 * @processor: A #UfoProcessor
 *
 * Returns: gpointer or %NULL if resources were not set before.
 */
gpointer
ufo_processor_get_command_queue (UfoProcessor *processor)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (processor);
    UFO_RESOURCES_CHECK_CLERR (clRetainCommandQueue (priv->cmd_queue));
    return priv->cmd_queue;
}

static void
ufo_processor_dispose (GObject *object)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (object);

    if (priv->resources) {
        g_object_unref (priv->resources);
        priv->resources = NULL;
    }

    if (priv->profiler) {
        g_object_unref (priv->profiler);
        priv->profiler = NULL;
    }

    G_OBJECT_CLASS (ufo_processor_parent_class)->dispose (object);
}

static void
ufo_processor_finalize (GObject *object)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (object);

    if (priv->cmd_queue) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseCommandQueue (priv->cmd_queue));
        priv->cmd_queue = NULL;
    }

    G_OBJECT_CLASS (ufo_processor_parent_class)->finalize (object);
}

static void
ufo_processor_setup_real (UfoProcessor *processor,
                          UfoResources *resources,
                          GError       **error)
{
    g_object_set (processor,
                  "ufo-resources", resources,
                  NULL);
}

static void
ufo_processor_configure_real (UfoProcessor *processor)
{
    g_warning ("%s: `configure' not implemented", G_OBJECT_TYPE_NAME (processor));
}

static void
ufo_processor_class_init (UfoProcessorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = ufo_processor_finalize;
    gobject_class->dispose = ufo_processor_dispose;
    
    g_type_class_add_private (gobject_class, sizeof (UfoProcessorPrivate));
    klass->setup = ufo_processor_setup_real;
    klass->configure = ufo_processor_configure_real;
}

static void
ufo_processor_init (UfoProcessor *self)
{
    UfoProcessorPrivate *priv = NULL;
    self->priv = priv = UFO_PROCESSOR_GET_PRIVATE (self);
    priv->resources = NULL;
    priv->profiler  = NULL;
    priv->cmd_queue = NULL;
}

static UfoCopyable *
ufo_processor_copy_real (gpointer origin,
                         gpointer _copy)
{
    UfoCopyable *copy;
    if (_copy)
        copy = UFO_COPYABLE (_copy);
    else
        copy = UFO_COPYABLE (ufo_processor_new());

    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (origin);
    ufo_processor_set_profiler (copy, priv->profiler);
    ufo_processor_set_resources (copy, priv->resources);
   
    return copy;
}

static void
ufo_copyable_interface_init (UfoCopyableIface *iface)
{
    iface->copy = ufo_processor_copy_real;
}

void
ufo_processor_setup (UfoProcessor *processor,
                     UfoResources *resources,
                     GError       **error)
{
    g_return_if_fail(UFO_IS_PROCESSOR (processor) &&
                     UFO_IS_RESOURCES (resources));

    UfoProcessorClass *klass = UFO_PROCESSOR_GET_CLASS (processor);
    ufo_processor_set_resources (processor, resources);
    klass->setup (processor, resources, error);
}

void
ufo_processor_configure (UfoProcessor *processor)
{
    g_return_if_fail (UFO_IS_PROCESSOR (processor));
    UfoProcessorClass *klass = UFO_PROCESSOR_GET_CLASS (processor);
    klass->configure(processor);
}
