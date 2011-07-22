#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-cl.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterClPrivate {
    /* add your private data here */
    cl_kernel kernel;
    gchar *file_name;
    gchar *kernel_name;
};

GType ufo_filter_cl_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterCl, ufo_filter_cl, UFO_TYPE_FILTER);

#define UFO_FILTER_CL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_CL, UfoFilterClPrivate))

enum {
    PROP_0,
    PROP_FILE_NAME,
    PROP_KERNEL,
    N_PROPERTIES
};

static GParamSpec *cl_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_cl_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
    /*UfoFilterCl *self = UFO_FILTER_CL(filter);*/
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_cl_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterClPrivate *priv = UFO_FILTER_CL_GET_PRIVATE(filter);
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(filter));
    cl_event event;

    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;

    ufo_resource_manager_add_program(manager, priv->file_name, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    cl_kernel kernel = ufo_resource_manager_get_kernel(manager, priv->kernel_name, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }

    size_t local_work_size[2] = { 16, 16 };
    size_t global_work_size[2];
    gint32 width, height;

    while (!ufo_buffer_is_finished(input)) {
        if (kernel) {
            ufo_buffer_get_dimensions(input, &width, &height);
            global_work_size[0] = (size_t) width;
            global_work_size[1] = (size_t) height;

            cl_mem buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(input, command_queue);
            cl_int err = CL_SUCCESS;

            err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &buffer_mem);
            /* XXX: For AMD CPU, a clFinish must be issued before enqueuing the
             * kernel. This could be moved to a ufo_kernel_launch method. */
            clFinish(command_queue);
            err = clEnqueueNDRangeKernel(command_queue,
                kernel,
                2, NULL, global_work_size, local_work_size,
                0, NULL, &event);
        }
        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
    g_async_queue_push(output_queue, input);

    /* free kernel */
}

static void ufo_filter_cl_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterCl *self = UFO_FILTER_CL(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_FILE_NAME:
            g_free(self->priv->file_name);
            self->priv->file_name = g_strdup(g_value_get_string(value));
            break;
        case PROP_KERNEL:
            g_free(self->priv->kernel_name);
            self->priv->kernel_name = g_strdup(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_cl_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterCl *self = UFO_FILTER_CL(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_FILE_NAME:
            g_value_set_string(value, self->priv->file_name);
            break;
        case PROP_KERNEL:
            g_value_set_string(value, self->priv->kernel_name);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_cl_class_init(UfoFilterClClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_cl_set_property;
    gobject_class->get_property = ufo_filter_cl_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_cl_initialize;
    filter_class->process = ufo_filter_cl_process;

    /* install properties */
    cl_properties[PROP_FILE_NAME] = 
        g_param_spec_string("file",
            "File in which the kernel resides",
            "File in which the kernel resides",
            "",
            G_PARAM_READWRITE);

    cl_properties[PROP_KERNEL] = 
        g_param_spec_string("kernel",
            "Kernel name",
            "Kernel name",
            "",
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_FILE_NAME, cl_properties[PROP_FILE_NAME]);
    g_object_class_install_property(gobject_class, PROP_KERNEL, cl_properties[PROP_KERNEL]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterClPrivate));
}

static void ufo_filter_cl_init(UfoFilterCl *self)
{
    UfoFilterClPrivate *priv = self->priv = UFO_FILTER_CL_GET_PRIVATE(self);
    priv->file_name = NULL;
    priv->kernel_name = NULL;
    priv->kernel = NULL;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_CL, NULL);
}
