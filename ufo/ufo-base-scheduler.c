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
#include "config.h"

#ifdef WITH_PYTHON
#include <Python.h>
#endif

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-arch-graph.h>
#include <ufo/ufo-base-scheduler.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-task-iface.h>
#include "ufo-priv.h"
#include "compat.h"


static void ufo_base_scheduler_initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (UfoBaseScheduler, ufo_base_scheduler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                ufo_base_scheduler_initable_iface_init))


#define UFO_BASE_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerPrivate))


struct _UfoBaseSchedulerPrivate {
    GError          *construct_error;
    UfoArchGraph    *arch;
    GList           *gpu_nodes;
    gboolean         expand;
    gboolean         trace;
    gboolean         rerun;
    gboolean         ran;
    gdouble          time;
};

enum {
    PROP_0,
    PROP_EXPAND,
    PROP_ENABLE_TRACING,
    PROP_ENABLE_RERUNS,
    PROP_TIME,
    N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * UfoBaseSchedulerError:
 * @UFO_BASE_SCHEDULER_ERROR_SETUP: Could not start scheduler due to error
 * @UFO_BASE_SCHEDULER_ERROR_EXECUTION: Error occured during execution
 */
GQuark
ufo_base_scheduler_error_quark (void)
{
    return g_quark_from_static_string ("ufo-scheduler-error-quark");
}

void
ufo_base_scheduler_run (UfoBaseScheduler *scheduler,
                        UfoTaskGraph *graph,
                        GError **error)
{
    UfoBaseSchedulerClass *klass;
    GTimer *timer;

    g_return_if_fail (UFO_IS_BASE_SCHEDULER (scheduler));

    klass = UFO_BASE_SCHEDULER_GET_CLASS (scheduler);

    g_return_if_fail (klass->run != NULL);

    if (!ufo_task_graph_is_alright (graph, error))
        return;

    timer = g_timer_new ();
    (*klass->run)(scheduler, graph, error);
    scheduler->priv->time = g_timer_elapsed (timer, NULL);

    g_timer_destroy (timer);
}

/**
 * ufo_base_scheduler_get_arch:
 * @scheduler: A #UfoBaseScheduler
 *
 * Get arch graph associated with this scheduler.
 *
 * Returns: (transfer none): The #UfoArchGraph object.
 */
UfoArchGraph *
ufo_base_scheduler_get_arch (UfoBaseScheduler *scheduler)
{
    g_return_val_if_fail (UFO_IS_BASE_SCHEDULER (scheduler), NULL);

    if (scheduler->priv->arch == NULL)
        scheduler->priv->arch = UFO_ARCH_GRAPH (ufo_arch_graph_new (NULL, NULL));

    return scheduler->priv->arch;
}

/**
 * ufo_base_scheduler_set_gpu_nodes:
 * @scheduler: A #UfoBaseScheduler
 * @arch: A #UfoArchGraph from which the nodes come from
 * @gpu_nodes: (element-type Ufo.GpuNode): A list of #UfoGpuNode objects.
 *
 * Sets the GPU nodes that @scheduler can only use. Note, that the #UfoGpuNode
 * objects must be from the same #UfoArchGraph that is returned by
 * ufo_base_scheduler_get_arch.
 */
void
ufo_base_scheduler_set_gpu_nodes (UfoBaseScheduler *scheduler,
                                  UfoArchGraph *arch,
                                  GList *gpu_nodes)
{
    g_return_if_fail (UFO_IS_BASE_SCHEDULER (scheduler));
    g_return_if_fail (arch != NULL);

    if (scheduler->priv->arch != NULL)
        g_object_unref (scheduler->priv->arch);

    scheduler->priv->arch = g_object_ref (arch);
    scheduler->priv->gpu_nodes = g_list_copy (gpu_nodes);
}

/**
 * ufo_base_scheduler_get_gpu_nodes:
 * @scheduler: A #UfoBaseScheduler
 *
 * Get the GPU nodes that @scheduler can use for execution.
 *
 * Returns: (transfer none) (element-type Ufo.GpuNode): A list of #UfoGpuNode objects.
 */
GList *
ufo_base_scheduler_get_gpu_nodes (UfoBaseScheduler *scheduler)
{
    g_return_val_if_fail (UFO_IS_BASE_SCHEDULER (scheduler), NULL);

    if (scheduler->priv->gpu_nodes != NULL)
        return scheduler->priv->gpu_nodes;

    return ufo_arch_graph_get_gpu_nodes (ufo_base_scheduler_get_arch (scheduler));
}

static void
ufo_base_scheduler_run_real (UfoBaseScheduler *scheduler,
                             UfoTaskGraph *graph,
                             GError **error)
{
    g_set_error (error, UFO_BASE_SCHEDULER_ERROR, UFO_BASE_SCHEDULER_ERROR_EXECUTION,
                 "UfoBaseScheduler::run not implemented");
}

static void
ufo_base_scheduler_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPAND:
            priv->expand = g_value_get_boolean (value);
            break;

        case PROP_ENABLE_TRACING:
            priv->trace = g_value_get_boolean (value);
            break;

        case PROP_ENABLE_RERUNS:
            priv->rerun = g_value_get_boolean (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_base_scheduler_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPAND:
            g_value_set_boolean (value, priv->expand);
            break;

        case PROP_ENABLE_TRACING:
            g_value_set_boolean (value, priv->trace);
            break;

        case PROP_ENABLE_RERUNS:
            g_value_set_boolean (value, priv->rerun);
            break;

        case PROP_TIME:
            g_value_set_double (value, priv->time);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_base_scheduler_dispose (GObject *object)
{
    UfoBaseSchedulerPrivate *priv;

    priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    if (priv->arch != NULL) {
        g_object_unref (priv->arch);
        priv->arch = NULL;
    }

    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->dispose (object);
}

static void
ufo_base_scheduler_finalize (GObject *object)
{
    UfoBaseSchedulerPrivate *priv;

    priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    g_clear_error (&priv->construct_error);

    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->finalize (object);
}

static gboolean
ufo_base_scheduler_initable_init (GInitable *initable,
                                  GCancellable *cancellable,
                                  GError **error)
{
    UfoBaseScheduler *scheduler;
    UfoBaseSchedulerPrivate *priv;

    g_return_val_if_fail (UFO_IS_BASE_SCHEDULER (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    scheduler = UFO_BASE_SCHEDULER (initable);
    priv = scheduler->priv;

    if (priv->construct_error != NULL) {
        if (error)
            *error = g_error_copy (priv->construct_error);

        return FALSE;
    }

    return TRUE;
}

static void
ufo_base_scheduler_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_base_scheduler_initable_init;
}

static void
ufo_base_scheduler_class_init (UfoBaseSchedulerClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    klass->run = ufo_base_scheduler_run_real;
    oclass->set_property = ufo_base_scheduler_set_property;
    oclass->get_property = ufo_base_scheduler_get_property;
    oclass->dispose = ufo_base_scheduler_dispose;
    oclass->finalize = ufo_base_scheduler_finalize;

    properties[PROP_EXPAND] =
        g_param_spec_boolean ("expand",
                              "Expand the task graph for better multi GPU performance",
                              "Expand the task graph for better multi GPU performance",
                              TRUE,
                              G_PARAM_READWRITE);

    properties[PROP_ENABLE_TRACING] =
        g_param_spec_boolean ("enable-tracing",
                              "Enable and write profile traces",
                              "Enable and write profile traces",
                              FALSE,
                              G_PARAM_READWRITE);

    properties[PROP_ENABLE_RERUNS] =
        g_param_spec_boolean ("enable-reruns",
                              "Enable additional runs of the scheduler",
                              "Enable additional runs of the scheduler",
                              FALSE,
                              G_PARAM_READWRITE);

    properties[PROP_TIME] =
        g_param_spec_double ("time",
                             "Finished execution time",
                             "Finished execution time in seconds",
                              0.0, G_MAXDOUBLE, 0.0,
                              G_PARAM_READABLE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (klass, sizeof (UfoBaseSchedulerPrivate));
}

static void
ufo_base_scheduler_init (UfoBaseScheduler *scheduler)
{
    UfoBaseSchedulerPrivate *priv;

    scheduler->priv = priv = UFO_BASE_SCHEDULER_GET_PRIVATE (scheduler);
    priv->expand = TRUE;
    priv->trace = FALSE;
    priv->rerun = FALSE;
    priv->ran = FALSE;
    priv->time = 0.0;
    priv->gpu_nodes = NULL;
}
