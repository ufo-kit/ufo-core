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
#include <ufo/ufo-profiler.h>

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

enum {
    PROP_0,
    PROP_UFO_RESOURCES,
    PROP_UFO_PROFILER,
    PROP_CL_COMMAND_QUEUE,
    N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoProcessor *
ufo_processor_new (void)
{
    return (UfoProcessor *) g_object_new (UFO_TYPE_PROCESSOR,
                                          NULL);
}

static void
ufo_processor_set_property (GObject      *object,
                            guint        property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (object);

    GObject *value_object;

    switch (property_id) {
        case PROP_UFO_RESOURCES:
            {
                value_object = g_value_get_object (value);

                if (priv->resources)
                    g_object_unref (priv->resources);

                if (value_object != NULL) {
                    priv->resources = g_object_ref (UFO_RESOURCES (value_object));
                }
            }
            break;
        case PROP_UFO_PROFILER:
            {
                value_object = g_value_get_object (value);

                if (priv->profiler)
                    g_object_unref (priv->profiler);

                if (value_object != NULL) {
                    priv->profiler = g_object_ref (UFO_PROFILER (value_object));
                }
            }
            break;
        case PROP_CL_COMMAND_QUEUE:
            if (priv->cmd_queue) {
                 UFO_RESOURCES_CHECK_CLERR (clReleaseCommandQueue (priv->cmd_queue));
            }
            priv->cmd_queue = g_value_get_pointer (value);
            UFO_RESOURCES_CHECK_CLERR (clRetainCommandQueue (priv->cmd_queue));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_processor_get_property (GObject    *object,
                            guint      property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    UfoProcessorPrivate *priv = UFO_PROCESSOR_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_UFO_RESOURCES:
            g_value_set_object (value, priv->resources);
            break;
        case PROP_UFO_PROFILER:
            g_value_set_object (value, priv->profiler);
            break;
        case PROP_CL_COMMAND_QUEUE:
            g_value_set_pointer (value, priv->cmd_queue);
            UFO_RESOURCES_CHECK_CLERR (clRetainCommandQueue (priv->cmd_queue));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
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
    gobject_class->set_property = ufo_processor_set_property;
    gobject_class->get_property = ufo_processor_get_property;

    properties[PROP_UFO_RESOURCES] =
        g_param_spec_object("ufo-resources",
                            "Pointer to the instance of UfoResources.",
                            "Pointer to the instance of UfoResources.",
                            UFO_TYPE_RESOURCES,
                            G_PARAM_READWRITE);

    properties[PROP_UFO_PROFILER] =
        g_param_spec_object("ufo-profiler",
                            "Pointer to the instance of UfoProfiler.",
                            "Pointer to the instance of UfoProfiler.",
                            UFO_TYPE_PROFILER,
                            G_PARAM_READWRITE);

    properties[PROP_CL_COMMAND_QUEUE] =
        g_param_spec_pointer("command-queue",
                             "Pointer to the instance of cl_command_queue.",
                             "Pointer to the instance of cl_command_queue.",
                             G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

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
    g_object_set (G_OBJECT (copy),
                  "ufo-resources", priv->resources,
                  "ufo-profiler", priv->profiler,
                  NULL);
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
    g_object_set (processor, "ufo-resources", resources, NULL);
    klass->setup (processor, resources, error);
}

void
ufo_processor_configure (UfoProcessor *processor)
{
    g_return_if_fail (UFO_IS_PROCESSOR (processor));
    UfoProcessorClass *klass = UFO_PROCESSOR_GET_CLASS (processor);
    klass->configure(processor);
}
