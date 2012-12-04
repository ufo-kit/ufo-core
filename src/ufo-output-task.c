/**
 * SECTION:ufo-output-task
 * @Short_description: Output task
 * @Title: Output task
 */

#include <gmodule.h>
#include <tiffio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo-output-task.h>
#include <ufo-cpu-task-iface.h>

struct _UfoOutputTaskPrivate {
    GAsyncQueue *out_queue;
    GAsyncQueue *in_queue;
    guint n_dims;
    guint n_copies;
};

static void ufo_task_interface_init (UfoTaskIface *iface);
static void ufo_cpu_task_interface_init (UfoCpuTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoOutputTask, ufo_output_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init)
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CPU_TASK,
                                                ufo_cpu_task_interface_init))

#define UFO_OUTPUT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_OUTPUT_TASK, UfoOutputTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_output_task_new (guint n_dims)
{
    UfoOutputTask *task = UFO_OUTPUT_TASK (g_object_new (UFO_TYPE_OUTPUT_TASK, NULL));

    task->priv->n_dims = n_dims;
    return UFO_NODE (task);
}

void
ufo_output_task_get_output_requisition (UfoOutputTask *task,
                                        UfoRequisition *requisition)
{
    UfoBuffer *buffer;
    UfoOutputTaskPrivate *priv;

    g_return_if_fail (UFO_IS_OUTPUT_TASK (task));
    priv = task->priv;
    buffer = g_async_queue_pop (priv->out_queue);
    ufo_buffer_get_requisition (buffer, requisition);
    g_async_queue_push (priv->out_queue, buffer);
}

UfoBuffer *
ufo_output_task_get_output_buffer (UfoOutputTask *task)
{
    g_return_val_if_fail (UFO_IS_OUTPUT_TASK (task), NULL);
    return g_async_queue_pop (task->priv->out_queue);
}

void
ufo_output_task_release_output_buffer (UfoOutputTask *task,
                                       UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_OUTPUT_TASK (task));
    g_async_queue_push (task->priv->in_queue, buffer);
}

static void
ufo_output_task_setup (UfoTask *task, GError **error)
{
}

static void
ufo_output_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition)
{
    (*requisition).n_dims = 0;
}

static void
ufo_output_task_get_structure (UfoTask *task,
                               guint *n_inputs,
                               UfoInputParameter **in_params)
{
    UfoOutputTaskPrivate *priv = UFO_OUTPUT_TASK_GET_PRIVATE (task);

    *n_inputs = 1;
    *in_params = g_new0 (UfoInputParameter, 1);
    (*in_params)[0].n_dims = priv->n_dims;
    (*in_params)[0].n_expected_items = -1;
}

static gboolean
ufo_output_task_process (UfoCpuTask *task,
                         UfoBuffer **outputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoOutputTaskPrivate *priv;
    UfoRequisition req;
    UfoBuffer *copy;

    g_return_val_if_fail (UFO_IS_OUTPUT_TASK (task), FALSE);
    ufo_buffer_get_requisition (outputs[0], &req);

    priv = UFO_OUTPUT_TASK_GET_PRIVATE (task);

    if (priv->n_copies == 0) {
        copy = ufo_buffer_dup (outputs[0]);
        g_async_queue_push (priv->in_queue, copy);
        priv->n_copies++;
    }

    copy = g_async_queue_pop (priv->in_queue);
    ufo_buffer_copy (outputs[0], copy);
    g_async_queue_push (priv->out_queue, copy);
    return TRUE;
}

static void
ufo_output_task_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_output_task_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_output_task_setup;
    iface->get_structure = ufo_output_task_get_structure;
    iface->get_requisition = ufo_output_task_get_requisition;
}

static void
ufo_cpu_task_interface_init (UfoCpuTaskIface *iface)
{
    iface->process = ufo_output_task_process;
}

static void
ufo_output_task_class_init (UfoOutputTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_output_task_set_property;
    gobject_class->get_property = ufo_output_task_get_property;

    g_type_class_add_private (gobject_class, sizeof(UfoOutputTaskPrivate));
}

static void
ufo_output_task_init(UfoOutputTask *self)
{
    self->priv = UFO_OUTPUT_TASK_GET_PRIVATE(self);
    self->priv->out_queue = g_async_queue_new ();
    self->priv->in_queue = g_async_queue_new ();
    self->priv->n_copies = 0;
}
