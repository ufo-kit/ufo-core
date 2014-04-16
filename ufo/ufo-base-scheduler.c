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

#include <ufo/ufo-base-scheduler.h>
#include <ufo/ufo-config.h>
#include <ufo/ufo-configurable.h>
#include <ufo/ufo-resources.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-task-iface.h>
#include "ufo-priv.h"
#include "compat.h"


static void ufo_base_scheduler_initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (UfoBaseScheduler, ufo_base_scheduler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                ufo_base_scheduler_initable_iface_init))


#define UFO_BASE_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerPrivate))


struct _UfoBaseSchedulerPrivate {
    GError          *construct_error;
    UfoConfig       *config;
    UfoResources    *resources;
    GList           *remotes;
    gboolean         expand;
    gboolean         trace;
    gboolean         rerun;
    gboolean         ran;
    gdouble          time;
};

enum {
    PROP_0,
    PROP_EXPAND,
    PROP_REMOTES,
    PROP_ENABLE_TRACING,
    PROP_ENABLE_RERUNS,
    PROP_TIME,
    N_PROPERTIES,

    /* Here come the overriden properties that we don't install ourselves. */
    PROP_CONFIG,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * UfoBaseSchedulerError:
 * @UFO_BASE_SCHEDULER_ERROR_SETUP: Could not start scheduler due to error
 */
GQuark
ufo_base_scheduler_error_quark (void)
{
    return g_quark_from_static_string ("ufo-scheduler-error-quark");
}

/**
 * ufo_base_scheduler_get_context:
 * @scheduler: A #UfoBaseScheduler
 *
 * Get the associated OpenCL context of @scheduler.
 *
 * Return value: (transfer full): An cl_context structure or %NULL on error.
 */
gpointer
ufo_base_scheduler_get_context (UfoBaseScheduler *scheduler)
{
    g_return_val_if_fail (UFO_IS_BASE_SCHEDULER (scheduler), NULL);
    return ufo_resources_get_context (scheduler->priv->resources);
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

    timer = g_timer_new ();
    (*klass->run)(scheduler, graph, error);
    scheduler->priv->time = g_timer_elapsed (timer, NULL);

    g_message ("Processing finished after %3.5fs", scheduler->priv->time);
    g_timer_destroy (timer);
}

/**
 * ufo_base_scheduler_get_resources:
 * @scheduler: A #UfoBaseScheduler
 *
 * Get resources associated with @scheduler.
 *
 * Returns: (transfer none): The #UfoResources object
 */
UfoResources *
ufo_base_scheduler_get_resources (UfoBaseScheduler *scheduler)
{
    g_return_val_if_fail (UFO_IS_BASE_SCHEDULER (scheduler), NULL);
    return scheduler->priv->resources;
}

/**
 * ufo_base_scheduler_get_remotes:
 * @scheduler: A #UfoBaseScheduler
 *
 * Returns: (transfer full) (element-type utf8): List with strings that the user
 * must free with g_list_free_full */
GList *
ufo_base_scheduler_get_remotes (UfoBaseScheduler *scheduler)
{
    GList *it;
    GList *remotes = NULL;

    g_return_val_if_fail (UFO_IS_BASE_SCHEDULER (scheduler), NULL);

    g_list_for (scheduler->priv->remotes, it) {
        remotes = g_list_append (remotes, g_strdup (it->data));
    }

    return remotes;
}

/**
 * ufo_base_scheduler_set_remotes:
 * @scheduler: A #UfoBaseScheduler
 * @remotes: (element-type utf8): List with remote addresses
 *
 * Set the used remotes for @scheduler.
 */
void
ufo_base_scheduler_set_remotes (UfoBaseScheduler *scheduler,
                                GList *remotes)
{
    UfoBaseSchedulerPrivate *priv;
    GList *it;

    g_return_if_fail (UFO_IS_BASE_SCHEDULER (scheduler));

    priv = scheduler->priv;
    g_list_free_full (priv->remotes, g_free);

    g_list_for (remotes, it) {
        priv->remotes = g_list_append (priv->remotes, g_strdup (it->data));
    }
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
copy_remote_list (UfoBaseSchedulerPrivate *priv,
                  GValueArray *array)
{
    if (priv->remotes != NULL) {
        g_list_foreach (priv->remotes, (GFunc) g_free, NULL);
        g_list_free (priv->remotes);
        priv->remotes = NULL;
    }

    for (guint i = 0; i < array->n_values; i++) {
        priv->remotes = g_list_append (priv->remotes,
                                       g_strdup (g_value_get_string (g_value_array_get_nth (array, i))));
    }
}

static void
ufo_base_scheduler_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_CONFIG:
            {
                GObject *vobject = g_value_get_object (value);

                if (vobject != NULL) {
                    if (priv->config != NULL)
                        g_object_unref (priv->config);

                    priv->config = UFO_CONFIG (vobject);
                    g_object_ref (priv->config);
                }
            }
            break;

        case PROP_REMOTES:
            copy_remote_list (priv, g_value_get_boxed (value));
            break;

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
ufo_base_scheduler_constructed (GObject *object)
{
    UfoBaseSchedulerPrivate *priv;

    priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);
    priv->resources = ufo_resources_new (priv->config,
                                         &priv->construct_error);
}

static void
ufo_base_scheduler_dispose (GObject *object)
{
    UfoBaseSchedulerPrivate *priv;

    priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    if (priv->config != NULL) {
        g_object_unref (priv->config);
        priv->config = NULL;
    }

    if (priv->resources != NULL) {
        g_object_unref (priv->resources);
        priv->resources = NULL;
    }

    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->dispose (object);
}

static void
ufo_base_scheduler_finalize (GObject *object)
{
    UfoBaseSchedulerPrivate *priv;

    priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    g_clear_error (&priv->construct_error);
    g_list_free_full (priv->remotes, g_free);

    priv->remotes = NULL;

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
    oclass->constructed  = ufo_base_scheduler_constructed;
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

    properties[PROP_REMOTES] =
        g_param_spec_value_array ("remotes",
                                  "List containing remote addresses",
                                  "List containing remote addresses of machines running ufod",
                                  g_param_spec_string ("remote",
                                                       "A remote address in the form tcp://addr:port",
                                                       "A remote address in the form tcp://addr:port (see http://api.zeromq.org/3-2:zmq-tcp)",
                                                       ".",
                                                       G_PARAM_READWRITE),
                                  G_PARAM_READWRITE);

    properties[PROP_TIME] =
        g_param_spec_double ("time",
                             "Finished execution time",
                             "Finished execution time in seconds",
                              0.0, G_MAXDOUBLE, 0.0,
                              G_PARAM_READABLE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_object_class_override_property (oclass, PROP_CONFIG, "config");

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
    priv->config = NULL;
    priv->resources = NULL;
    priv->remotes = NULL;
    priv->time = 0.0;
}
