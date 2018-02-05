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

#include "ufo-task-iface.h"
#include "ufo-task-node.h"

/**
 * SECTION:ufo-task-iface
 * @Short_description: Base interface of all tasks
 * @Title: UfoTaskIface
 *
 * Interface that defines the behaviour of all tasks. Each scheduler uses the
 * same policy to run a task: First, ufo_task_setup(), ufo_task_get_num_inputs()
 * and ufo_task_get_num_dimensions() is called for each tasks. Then in each
 * iteration the task is asked about its size requirements using
 * ufo_task_get_requisition() and then executed using ufo_task_process() and/or
 * ufo_task_generate().
 */

typedef UfoTaskIface UfoTaskInterface;

G_DEFINE_INTERFACE (UfoTask, ufo_task, G_TYPE_OBJECT)

enum {
    PROCESSED,
    GENERATED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/**
 * UfoTaskError:
 * @UFO_TASK_ERROR_SETUP: Error during setup of a task.
 */
GQuark
ufo_task_error_quark ()
{
    return g_quark_from_static_string ("ufo-task-error-quark");
}

void
ufo_task_setup (UfoTask *task,
                UfoResources *resources,
                GError **error)
{
    GError *tmp_error = NULL;

    ufo_task_node_setup (UFO_TASK_NODE (task));
    UFO_TASK_GET_IFACE (task)->setup (task, resources, &tmp_error);

    if (tmp_error != NULL) {
        g_propagate_prefixed_error (error, tmp_error,
                                    "%s: ", ufo_task_node_get_plugin_name (UFO_TASK_NODE (task)));
    }
}

void
ufo_task_get_requisition (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoRequisition *requisition)
{
    UFO_TASK_GET_IFACE (task)->get_requisition (task, inputs, requisition);
}

guint
ufo_task_get_num_inputs (UfoTask *task)
{
    return UFO_TASK_GET_IFACE (task)->get_num_inputs (task);
}

guint
ufo_task_get_num_dimensions (UfoTask *task,
                             guint input)
{
    return UFO_TASK_GET_IFACE (task)->get_num_dimensions (task, input);
}

UfoTaskMode
ufo_task_get_mode (UfoTask *task)
{
    return UFO_TASK_GET_IFACE (task)->get_mode (task);
}

void
ufo_task_set_json_object_property (UfoTask *task,
                                   const gchar *prop_name,
                                   JsonObject *object)
{
    UFO_TASK_GET_IFACE (task)->set_json_object_property (task, prop_name, object);
}

static void
emit_signal (gpointer instance, guint signal_id, GQuark detail)
{
#ifdef WITH_PYTHON
    if (Py_IsInitialized ()) {
        PyGILState_STATE state = PyGILState_Ensure ();
#endif

    g_signal_emit (instance, signal_id, detail, G_TYPE_NONE);

#ifdef WITH_PYTHON
        PyGILState_Release (state);
    }
    else {
        g_signal_emit (instance, signal_id, detail, G_TYPE_NONE);
    }
#endif
}

gboolean
ufo_task_process (UfoTask *task,
                  UfoBuffer **inputs,
                  UfoBuffer *output,
                  UfoRequisition *requisition)
{
    UfoProfiler *profiler;
    gboolean result;

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_trace_event (profiler, UFO_TRACE_EVENT_PROCESS | UFO_TRACE_EVENT_BEGIN);
    result = UFO_TASK_GET_IFACE (task)->process (task, inputs, output, requisition);
    ufo_profiler_trace_event (profiler, UFO_TRACE_EVENT_PROCESS | UFO_TRACE_EVENT_END);

    emit_signal (task, signals[PROCESSED], 0);
    ufo_task_node_increase_processed (UFO_TASK_NODE (task));

    return result;
}

gboolean
ufo_task_generate (UfoTask *task,
                   UfoBuffer *output,
                   UfoRequisition *requisition)
{
    UfoProfiler *profiler;
    gboolean result;

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE(task));
    ufo_profiler_trace_event (profiler, UFO_TRACE_EVENT_GENERATE | UFO_TRACE_EVENT_BEGIN);
    result = UFO_TASK_GET_IFACE (task)->generate (task, output, requisition);
    ufo_profiler_trace_event (profiler, UFO_TRACE_EVENT_GENERATE | UFO_TRACE_EVENT_END);

    emit_signal (task, signals[GENERATED], 0);

    return result;
}

gboolean
ufo_task_uses_gpu (UfoTask *task)
{
    return ufo_task_get_mode (task) & UFO_TASK_MODE_GPU;
}

gboolean
ufo_task_uses_cpu (UfoTask *task)
{
    return ufo_task_get_mode (task) & UFO_TASK_MODE_CPU;
}

static void
warn_unimplemented (UfoTask *task,
                    const gchar *func)
{
    g_warning ("%s: `%s' not implemented", G_OBJECT_TYPE_NAME (task), func);
}

static void
ufo_task_set_json_object_property_real (UfoTask *task,
                                        const gchar *prop_name,
                                        JsonObject *object)
{
    warn_unimplemented (task, "set_json_object_property");
}

static void
ufo_task_setup_real (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
    warn_unimplemented (task, "setup");
}

static void
ufo_task_get_requisition_real (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition)
{
    warn_unimplemented (task, "get_allocation");
}

static guint
ufo_task_get_num_inputs_real (UfoTask *task)
{
    warn_unimplemented (task, "get_num_inputs");
    return 0;
}

static guint
ufo_task_get_num_dimensions_real (UfoTask *task,
                                  guint input)
{
    warn_unimplemented (task, "get_num_dimensions");
    return 0;
}

static UfoTaskMode
ufo_task_get_mode_real (UfoTask *task)
{
    warn_unimplemented (task, "get_mode");
    return 0;
}

static gboolean
ufo_task_process_real (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    warn_unimplemented (task, "process");
    return FALSE;
}

static gboolean
ufo_task_generate_real (UfoTask *task,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    warn_unimplemented (task, "generate");
    return FALSE;
}

static void
ufo_task_default_init (UfoTaskInterface *iface)
{
    iface->setup = ufo_task_setup_real;
    iface->get_requisition = ufo_task_get_requisition_real;
    iface->get_num_inputs = ufo_task_get_num_inputs_real;
    iface->get_num_dimensions = ufo_task_get_num_dimensions_real;
    iface->get_mode = ufo_task_get_mode_real;
    iface->set_json_object_property = ufo_task_set_json_object_property_real;
    iface->process = ufo_task_process_real;
    iface->generate = ufo_task_generate_real;

    signals[PROCESSED] =
        g_signal_new ("processed",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                      0,
                      NULL, NULL, g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[GENERATED] =
        g_signal_new ("generated",
                      G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                      0,
                      NULL, NULL, g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}
