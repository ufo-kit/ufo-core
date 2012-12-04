/**
 * SECTION:ufo-remote-task
 * @Short_description: Encapsulate remote task
 * @Title: Remote task
 */

#include <gmodule.h>
#include <tiffio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo-remote-task.h>
#include <ufo-remote-node.h>
#include <ufo-cpu-task-iface.h>

struct _UfoRemoteTaskPrivate {
    UfoRemoteNode *remote;
};

static void ufo_task_interface_init (UfoTaskIface *iface);
static void ufo_cpu_task_interface_init (UfoCpuTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRemoteTask, ufo_remote_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init)
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CPU_TASK,
                                                ufo_cpu_task_interface_init))

#define UFO_REMOTE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REMOTE_TASK, UfoRemoteTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_remote_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REMOTE_TASK, NULL));
}

static void
ufo_remote_task_setup (UfoTask *task, GError **error)
{
    UfoRemoteTaskPrivate *priv;

    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));
    priv->remote = UFO_REMOTE_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    g_assert (priv->remote != NULL);
    g_object_ref (priv->remote);
    ufo_remote_node_request_setup (priv->remote);
}

static void
ufo_remote_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition)
{
    UfoRemoteTaskPrivate *priv;

    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));

    /*
     * We send our input to the remote node which will execute immediately.
     * After remote execution, we will know the requisition of the _last_ remote
     * task node and can get it back.
     */
    ufo_remote_node_send_inputs (priv->remote, inputs);
    ufo_remote_node_get_requisition (priv->remote, requisition);
}

static void
ufo_remote_task_get_structure (UfoTask *task,
                               guint *n_inputs,
                               UfoInputParameter **in_params)
{
    UfoRemoteTaskPrivate *priv;

    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));
    ufo_remote_node_get_structure (priv->remote, n_inputs, in_params);
}

static gboolean
ufo_remote_task_process (UfoCpuTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoRemoteTaskPrivate *priv;
    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));

    ufo_remote_node_get_result (priv->remote, output);
    return TRUE;
}

static void
ufo_remote_task_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_remote_task_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_remote_task_dispose (GObject *object)
{
    UfoRemoteTaskPrivate *priv;

    priv = UFO_REMOTE_TASK_GET_PRIVATE (object);
    g_object_unref (priv->remote);
    priv->remote = NULL;
    G_OBJECT_CLASS (ufo_remote_task_parent_class)->dispose (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_remote_task_setup;
    iface->get_structure = ufo_remote_task_get_structure;
    iface->get_requisition = ufo_remote_task_get_requisition;
}

static void
ufo_cpu_task_interface_init (UfoCpuTaskIface *iface)
{
    iface->process = ufo_remote_task_process;
}

static void
ufo_remote_task_class_init (UfoRemoteTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_remote_task_set_property;
    oclass->get_property = ufo_remote_task_get_property;
    oclass->dispose = ufo_remote_task_dispose;

    g_type_class_add_private (oclass, sizeof(UfoRemoteTaskPrivate));
}

static void
ufo_remote_task_init(UfoRemoteTask *self)
{
    self->priv = UFO_REMOTE_TASK_GET_PRIVATE(self);
}
