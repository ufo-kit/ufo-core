#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-complex.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"

typedef enum {
    OP_ADD = 0,
    OP_MUL,
    OP_DIV,
    OP_N
} ComplexOperation;

static const gchar *operation_map[] = { "add", "mul", "div" };

struct _UfoFilterComplexPrivate {
     cl_kernel kernels[OP_N];
     ComplexOperation operation;
};

GType ufo_filter_complex_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterComplex, ufo_filter_complex, UFO_TYPE_FILTER);

#define UFO_FILTER_COMPLEX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_COMPLEX, UfoFilterComplexPrivate))

enum {
    PROP_0,
    PROP_OP,
    N_PROPERTIES
};

static GParamSpec *complex_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_complex_initialize(UfoFilter *filter)
{
    UfoFilterComplex *self = UFO_FILTER_COMPLEX(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;

    ufo_resource_manager_add_program(manager, "complex.cl", NULL, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernels[OP_ADD] = ufo_resource_manager_get_kernel(manager, "c_add", &error);
    self->priv->kernels[OP_MUL] = ufo_resource_manager_get_kernel(manager, "c_mul", &error);
    self->priv->kernels[OP_DIV] = ufo_resource_manager_get_kernel(manager, "c_div", &error);
}

static void ufo_filter_complex_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    
    UfoResourceManager *manager = ufo_resource_manager();
    UfoFilterComplex *self = UFO_FILTER_COMPLEX(filter);

    UfoChannel *input_channel_a = ufo_filter_get_input_channel_by_name(filter, "input1");
    UfoChannel *input_channel_b = ufo_filter_get_input_channel_by_name(filter, "input2");
    UfoChannel *output_channel = ufo_filter_get_output_channel(filter);
    
    UfoBuffer *a = ufo_channel_pop(input_channel_a);
    UfoBuffer *b = ufo_channel_pop(input_channel_b);
    gint wa, ha, wb, hb;
    gint dimensions[4] = { 1, 1, 1, 1 };
    
    cl_kernel kernel = self->priv->kernels[self->priv->operation];
    cl_command_queue cmd_queue = ufo_filter_get_command_queue(filter);
    cl_mem mem_a, mem_b, mem_r;
    cl_event event;
    
    size_t global_work_size[2] = { 0, 0 };

    while ((a != NULL) && (b != NULL)) {
        ufo_buffer_get_2d_dimensions(a, &wa, &ha);
        ufo_buffer_get_2d_dimensions(b, &wb, &hb);
        g_assert(wa == wb);
        g_assert(ha == hb);
        
        dimensions[0] = wa;
        dimensions[1] = ha;
        UfoBuffer *r = ufo_resource_manager_request_buffer(manager, UFO_BUFFER_2D, dimensions, NULL, TRUE);
        
        mem_a = ufo_buffer_get_gpu_data(a, cmd_queue);
        mem_b = ufo_buffer_get_gpu_data(b, cmd_queue);
        mem_r = ufo_buffer_get_gpu_data(a, cmd_queue);
        
        /* Each thread processes the real and the imaginary part */
        global_work_size[0] = wa / 2;
        global_work_size[1] = ha;
        clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &mem_a);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &mem_b);
        clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *) &mem_r);
        clEnqueueNDRangeKernel(cmd_queue, kernel,
                2, NULL, global_work_size, NULL,
                0, NULL, &event);

        ufo_filter_account_gpu_time(filter, (void **) &event);
        clReleaseEvent(event);

        ufo_channel_push(output_channel, r);
        ufo_resource_manager_release_buffer(manager, a);
        ufo_resource_manager_release_buffer(manager, b);
        a = ufo_channel_pop(input_channel_a);
        b = ufo_channel_pop(input_channel_b);
    }
    ufo_channel_finish(output_channel);
}

static void ufo_filter_complex_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterComplex *self = UFO_FILTER_COMPLEX(object);
    gchar *op_string = NULL;

    switch (property_id) {
        case PROP_OP:
            op_string = g_strdup(g_value_get_string(value));
            for (int i = 0; i < OP_N; i++) {
                if (!g_strcmp0(op_string, operation_map[i])) {
                    self->priv->operation = i;
                    g_free(op_string);
                    return;
                }
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_complex_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterComplex *self = UFO_FILTER_COMPLEX(object);

    switch (property_id) {
        case PROP_OP:
            g_value_set_string(value, operation_map[self->priv->operation]);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_complex_class_init(UfoFilterComplexClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_complex_set_property;
    gobject_class->get_property = ufo_filter_complex_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_complex_initialize;
    filter_class->process = ufo_filter_complex_process;

    /* install properties */
    complex_properties[PROP_OP] = 
        g_param_spec_string("operation",
            "Complex operation from [\"add\", \"mul\", \"div\"]",
            "Complex operation from [\"add\", \"mul\", \"div\"]",
            "add",
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_OP, complex_properties[PROP_OP]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterComplexPrivate));
}

static void ufo_filter_complex_init(UfoFilterComplex *self)
{
    UfoFilterComplexPrivate *priv = self->priv = UFO_FILTER_COMPLEX_GET_PRIVATE(self);
    priv->operation = OP_ADD;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_COMPLEX, NULL);
}
