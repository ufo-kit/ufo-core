/**
 * SECTION:ufo-dummy-task
 * @Short_description: Dummy task
 * @Title: UfoDummyTask
 */

#include <gmodule.h>
#include <tiffio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo-dummy-task.h>

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoDummyTask, ufo_dummy_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_DUMMY_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DUMMY_TASK, UfoDummyTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_dummy_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_DUMMY_TASK, NULL));
}

static void
ufo_dummy_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
}

static void
ufo_dummy_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition)
{
}

static void
ufo_dummy_task_get_structure (UfoTask *task,
                              guint *n_inputs,
                              UfoInputParam **in_params,
                              UfoTaskMode *mode)
{
}


static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_dummy_task_setup;
    iface->get_structure = ufo_dummy_task_get_structure;
    iface->get_requisition = ufo_dummy_task_get_requisition;
}

static void
ufo_dummy_task_class_init (UfoDummyTaskClass *klass)
{
}

static void
ufo_dummy_task_init (UfoDummyTask *task)
{
    ufo_task_node_set_plugin_name (UFO_TASK_NODE (task), "[dummy]");
}
