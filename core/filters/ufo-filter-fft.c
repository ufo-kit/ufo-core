#include <gmodule.h>
#include <CL/cl.h>
#include <clFFT.h>

#include "config.h"
#include "ufo-resource-manager.h"
#include "ufo-filter-fft.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterFFTPrivate {
    cl_kernel kernel;
    clFFT_Dimension fft_dimensions;
    clFFT_Dim3 fft_size;
};

GType ufo_filter_fft_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterFFT, ufo_filter_fft, UFO_TYPE_FILTER);

#define UFO_FILTER_FFT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_FFT, UfoFilterFFTPrivate))

enum {
    PROP_0,
    PROP_DIMENSIONS,
    PROP_SIZE_X,
    PROP_SIZE_Y,
    PROP_SIZE_Z,
    N_PROPERTIES
};

static GParamSpec *fft_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

static guint32 pow2round(guint32 x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

/* 
 * virtual methods 
 */
static void ufo_filter_fft_initialize(UfoFilter *filter)
{
    UfoFilterFFT *self = UFO_FILTER_FFT(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "fft.cl", NULL, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "fft_spread", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_fft_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterFFTPrivate *priv = UFO_FILTER_FFT_GET_PRIVATE(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));
    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(filter));

    int err = CL_SUCCESS;
    clFFT_Plan fft_plan = NULL;
    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    gint32 dimensions[4] = { 1, 1, 1, 1 }, width, height;

    while (!ufo_buffer_is_finished(input)) {
        ufo_buffer_get_2d_dimensions(input, &width, &height);

        /* Check if we can re-use the old FFT plan */
        if (priv->fft_size.x != pow2round(width)) {
            priv->fft_size.x = pow2round(width);
            if (priv->fft_dimensions == clFFT_2D)
                priv->fft_size.y = priv->fft_size.x; //pow2round(height);
            clFFT_DestroyPlan(fft_plan);
            fft_plan = NULL;
        }

        /* No, then create a new one */
        if (fft_plan == NULL) {
            fft_plan = clFFT_CreatePlan(
                    (cl_context) ufo_resource_manager_get_context(manager),
                    priv->fft_size, priv->fft_dimensions,
                    clFFT_InterleavedComplexFormat, &err);
        }

        /* 1. Spread data for interleaved FFT */
        UfoBuffer *fft_buffer = NULL;
        
        dimensions[0] = 2 * priv->fft_size.x;
        dimensions[1] = priv->fft_dimensions == clFFT_1D ? height : priv->fft_size.y;
        fft_buffer = ufo_resource_manager_request_buffer(manager, UFO_BUFFER_2D, dimensions, NULL, FALSE);

        cl_mem fft_buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(fft_buffer, command_queue);
        cl_mem sinogram_mem = (cl_mem) ufo_buffer_get_gpu_data(input, command_queue);
        cl_event event, wait_on_event;
        size_t global_work_size[2];

        global_work_size[0] = priv->fft_size.x;
        global_work_size[1] = priv->fft_dimensions == clFFT_1D ? height : priv->fft_size.y;
        clSetKernelArg(priv->kernel, 0, sizeof(cl_mem), (void *) &fft_buffer_mem);
        clSetKernelArg(priv->kernel, 1, sizeof(cl_mem), (void *) &sinogram_mem);
        clSetKernelArg(priv->kernel, 2, sizeof(int), &width);
        clSetKernelArg(priv->kernel, 3, sizeof(int), &height);
        clEnqueueNDRangeKernel(command_queue,
                priv->kernel, 
                2, NULL, global_work_size, NULL, 
                0, NULL, &wait_on_event);
        
        ufo_filter_account_gpu_time(filter, (void **) &wait_on_event);

        /* FIXME: we should wait for previous computations */
        if (priv->fft_dimensions == clFFT_1D)
            clFFT_ExecuteInterleaved(command_queue,
                fft_plan, height, clFFT_Forward, 
                fft_buffer_mem, fft_buffer_mem,
                1, &wait_on_event, &event);
        else
            clFFT_ExecuteInterleaved(command_queue,
                fft_plan, 1, clFFT_Forward, 
                fft_buffer_mem, fft_buffer_mem,
                1, &wait_on_event, &event);

        /* XXX: FFT execution does _not_ return event */
        /*ufo_filter_account_gpu_time(filter, (void **) &event);*/
        /*clReleaseEvent(event);*/

        ufo_buffer_transfer_id(input, fft_buffer);
        ufo_resource_manager_release_buffer(manager, input);

        g_async_queue_push(output_queue, fft_buffer);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }

    g_async_queue_push(output_queue, input);
    clFFT_DestroyPlan(fft_plan);
}

static void ufo_filter_fft_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterFFT *self = UFO_FILTER_FFT(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_DIMENSIONS:
            switch(g_value_get_int(value)) {
                case 1:
                    self->priv->fft_dimensions = clFFT_1D;
                    break;
                case 2:
                    self->priv->fft_dimensions = clFFT_2D;
                    break;
                case 3:
                    self->priv->fft_dimensions = clFFT_3D;
                    break;
            }
            break;
        case PROP_SIZE_X:
            self->priv->fft_size.x = g_value_get_int(value);
            break;
        case PROP_SIZE_Y:
            self->priv->fft_size.y = g_value_get_int(value);
            break;
        case PROP_SIZE_Z:
            self->priv->fft_size.z = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_fft_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterFFT *self = UFO_FILTER_FFT(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_DIMENSIONS:
            switch (self->priv->fft_dimensions) {
                case clFFT_1D:
                    g_value_set_int(value, 1);
                    break;
                case clFFT_2D:
                    g_value_set_int(value, 2);
                    break;
                case clFFT_3D:
                    g_value_set_int(value, 3);
                    break;
            }
            break;
        case PROP_SIZE_X:
            g_value_set_int(value, self->priv->fft_size.x);
            break;
        case PROP_SIZE_Y:
            g_value_set_int(value, self->priv->fft_size.y);
            break;
        case PROP_SIZE_Z:
            g_value_set_int(value, self->priv->fft_size.z);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_fft_class_init(UfoFilterFFTClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_fft_set_property;
    gobject_class->get_property = ufo_filter_fft_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_fft_initialize;
    filter_class->process = ufo_filter_fft_process;

    /* install properties */
    fft_properties[PROP_DIMENSIONS] = 
        g_param_spec_int("dimensions",
            "Number of FFT dimensions from 1 to 3",
            "Number of FFT dimensions from 1 to 3",
            1,   /* minimum */
            3,   /* maximum */
            1,   /* default */
            G_PARAM_READWRITE);

    fft_properties[PROP_SIZE_X] = 
        g_param_spec_int("size-x",
            "Size of the FFT transform in x-direction",
            "Size of the FFT transform in x-direction",
            1,      /* minimum */
            8192,   /* maximum */
            1,      /* default */
            G_PARAM_READWRITE);

    fft_properties[PROP_SIZE_Y] = 
        g_param_spec_int("size-y",
            "Size of the FFT transform in y-direction",
            "Size of the FFT transform in y-direction",
            1,      /* minimum */
            8192,   /* maximum */
            1,      /* default */
            G_PARAM_READWRITE);

    fft_properties[PROP_SIZE_Z] = 
        g_param_spec_int("size-z",
            "Size of the FFT transform in z-direction",
            "Size of the FFT transform in z-direction",
            1,      /* minimum */
            8192,   /* maximum */
            1,      /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_DIMENSIONS, fft_properties[PROP_DIMENSIONS]);
    g_object_class_install_property(gobject_class, PROP_SIZE_X, fft_properties[PROP_SIZE_X]);
    g_object_class_install_property(gobject_class, PROP_SIZE_Y, fft_properties[PROP_SIZE_Y]);
    g_object_class_install_property(gobject_class, PROP_SIZE_Z, fft_properties[PROP_SIZE_Z]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterFFTPrivate));
}

static void ufo_filter_fft_init(UfoFilterFFT *self)
{
    UfoFilterFFTPrivate *priv = self->priv = UFO_FILTER_FFT_GET_PRIVATE(self);
    priv->fft_dimensions = clFFT_1D;
    priv->fft_size.x = 1;
    priv->fft_size.y = 1;
    priv->fft_size.z = 1;
    priv->kernel = NULL;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_FFT, NULL);
}
