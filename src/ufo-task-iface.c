#include "ufo-task-iface.h"

typedef UfoTaskIface UfoTaskInterface;

G_DEFINE_INTERFACE (UfoTask, ufo_task, G_TYPE_OBJECT)

void
ufo_task_setup (UfoTask *task,
                GError **error)
{
    UFO_TASK_GET_IFACE (task)->setup (task, error);
}

void
ufo_task_get_requisition (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoRequisition *requisition)
{
    UFO_TASK_GET_IFACE (task)->get_requisition (task, inputs, requisition);
}

void
ufo_task_get_structure (UfoTask *task,
                        guint *n_inputs,
                        UfoInputParameter **in_params)
{
    UFO_TASK_GET_IFACE (task)->get_structure (task, n_inputs, in_params);
}

static void
ufo_task_setup_real (UfoTask *task,
                     GError **error)
{
    g_warning ("`setup' not implemented");
}

static void
ufo_task_get_requisition_real (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition)
{
    g_warning ("`get_allocation' not implemented");
}

static void
ufo_task_get_structure_real (UfoTask *task,
                             guint *n_inputs,
                             UfoInputParameter **in_params)
{
    g_warning ("`get_structure' not implemented");
}

static void
ufo_task_default_init (UfoTaskInterface *iface)
{
    iface->setup = ufo_task_setup_real;
    iface->get_requisition = ufo_task_get_requisition_real;
    iface->get_structure = ufo_task_get_structure_real;
}
