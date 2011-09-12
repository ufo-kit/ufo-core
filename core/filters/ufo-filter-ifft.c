#include <gmodule.h>
#include <CL/cl.h>
#include <clFFT.h>

#include "config.h"
#include "ufo-resource-manager.h"
#include "ufo-filter-ifft.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"

struct _UfoFilterIFFTPrivate {
    cl_kernel pack_kernel;
    cl_kernel normalize_kernel;
    clFFT_Dimension ifft_dimensions;
    clFFT_Dim3 ifft_size;
    gint32 final_width;
    gint32 final_height;
};

GType ufo_filter_ifft_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterIFFT, ufo_filter_ifft, UFO_TYPE_FILTER);

#define UFO_FILTER_IFFT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_IFFT, UfoFilterIFFTPrivate))

enum {
    PROP_0,
    PROP_DIMENSIONS,
    PROP_SIZE_X,
    PROP_SIZE_Y,
    PROP_SIZE_Z,
    PROP_FINAL_WIDTH,
    PROP_FINAL_HEIGHT,
    N_PROPERTIES
};

static GParamSpec *ifft_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_ifft_initialize(UfoFilter *filter)
{
    UfoFilterIFFT *self = UFO_FILTER_IFFT(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    ufo_resource_manager_add_program(manager, "fft.cl", NULL, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->pack_kernel = ufo_resource_manager_get_kernel(manager, "fft_pack", &error);
    self->priv->normalize_kernel = ufo_resource_manager_get_kernel(manager, "fft_normalize", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_ifft_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterIFFTPrivate *priv = UFO_FILTER_IFFT_GET_PRIVATE(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    UfoChannel *input_channel = ufo_filter_get_input_channel(filter);
    UfoChannel *output_channel = ufo_filter_get_output_channel(filter);
    cl_command_queue command_queue = (cl_command_queue) ufo_filter_get_command_queue(filter);

    int err = CL_SUCCESS;
    clFFT_Plan ifft_plan = NULL;
    UfoBuffer *input = ufo_channel_pop(input_channel);
    gint32 dimensions[4] = { 1, 1, 1, 1 };

    while (input != NULL) {
        ufo_buffer_get_dimensions(input, dimensions);
        gint32 width = dimensions[0];
        gint32 height = dimensions[1];

        if (priv->ifft_size.x != width / 2) {
            priv->ifft_size.x = width / 2;
            if (priv->ifft_dimensions == clFFT_2D)
                priv->ifft_size.y = height;
            clFFT_DestroyPlan(ifft_plan);
            ifft_plan = NULL;
        }

        if (ifft_plan == NULL) {
            ifft_plan = clFFT_CreatePlan(
                (cl_context) ufo_resource_manager_get_context(manager),
                priv->ifft_size, priv->ifft_dimensions,
                clFFT_InterleavedComplexFormat, &err);
        }

        cl_mem fft_buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(input, command_queue);
        cl_event event;
        cl_event wait_on_event;

        /* 1. Inverse FFT */
        if (priv->ifft_dimensions == clFFT_1D)
            clFFT_ExecuteInterleaved(command_queue,
                    ifft_plan, height, clFFT_Inverse, 
                    fft_buffer_mem, fft_buffer_mem,
                    0, NULL, &wait_on_event);
        else
            clFFT_ExecuteInterleaved(command_queue,
                    ifft_plan, 1, clFFT_Inverse, 
                    fft_buffer_mem, fft_buffer_mem,
                    0, NULL, &wait_on_event);

        /* XXX: FFT execution does _not_ return event */
        /*ufo_filter_account_gpu_time(filter, &wait_on_event);*/

        /* 2. Pack interleaved complex numbers */
        size_t global_work_size[2];
        global_work_size[0] = priv->ifft_size.x;

        width = (priv->final_width == -1) ? priv->ifft_size.x : priv->final_width;
        height = (priv->final_height == -1) ? height : priv->final_height;
        dimensions[0] = width;
        dimensions[1] = height;
        UfoBuffer *sinogram = ufo_resource_manager_request_buffer(manager,
                UFO_BUFFER_2D, dimensions, NULL, FALSE);
        global_work_size[1] = height;

        cl_mem sinogram_mem = (cl_mem) ufo_buffer_get_gpu_data(sinogram, command_queue);
        clSetKernelArg(priv->normalize_kernel, 0, sizeof(cl_mem), (void *) &fft_buffer_mem);
        clEnqueueNDRangeKernel(command_queue,
                priv->normalize_kernel,
                2, NULL, global_work_size, NULL,
                0, NULL, &event);

        ufo_filter_account_gpu_time(filter, (void **) &event);
        clReleaseEvent(event);

        clSetKernelArg(priv->pack_kernel, 0, sizeof(cl_mem), (void *) &fft_buffer_mem);
        clSetKernelArg(priv->pack_kernel, 1, sizeof(cl_mem), (void *) &sinogram_mem);
        clSetKernelArg(priv->pack_kernel, 2, sizeof(int), &width);

        clEnqueueNDRangeKernel(command_queue,
                priv->pack_kernel,
                2, NULL, global_work_size, NULL,
                0, NULL, &event);

        ufo_filter_account_gpu_time(filter, (void **) &event);
        clReleaseEvent(event);

        ufo_buffer_transfer_id(input, sinogram);
        ufo_resource_manager_release_buffer(manager, input);

        ufo_channel_push(output_channel, sinogram);
        input = ufo_channel_pop(input_channel);
    }
    ufo_channel_finish(output_channel);
    clFFT_DestroyPlan(ifft_plan);
}

static void ufo_filter_ifft_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterIFFT *self = UFO_FILTER_IFFT(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_DIMENSIONS:
            switch(g_value_get_int(value)) {
                case 1:
                    self->priv->ifft_dimensions = clFFT_1D;
                    break;
                case 2:
                    self->priv->ifft_dimensions = clFFT_2D;
                    break;
                case 3:
                    self->priv->ifft_dimensions = clFFT_3D;
                    break;
            }
            break;
        case PROP_SIZE_X:
            self->priv->ifft_size.x = g_value_get_int(value);
            break;
        case PROP_SIZE_Y:
            self->priv->ifft_size.y = g_value_get_int(value);
            break;
        case PROP_SIZE_Z:
            self->priv->ifft_size.z = g_value_get_int(value);
            break;
        case PROP_FINAL_WIDTH:
            self->priv->final_width = g_value_get_int(value);
            break;
        case PROP_FINAL_HEIGHT:
            self->priv->final_height = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_ifft_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterIFFT *self = UFO_FILTER_IFFT(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_DIMENSIONS:
            switch (self->priv->ifft_dimensions) {
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
            g_value_set_int(value, self->priv->ifft_size.x);
            break;
        case PROP_SIZE_Y:
            g_value_set_int(value, self->priv->ifft_size.y);
            break;
        case PROP_SIZE_Z:
            g_value_set_int(value, self->priv->ifft_size.z);
            break;
        case PROP_FINAL_WIDTH:
            g_value_set_int(value, self->priv->final_width);
            break;
        case PROP_FINAL_HEIGHT:
            g_value_set_int(value, self->priv->final_height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_ifft_class_init(UfoFilterIFFTClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_ifft_set_property;
    gobject_class->get_property = ufo_filter_ifft_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_ifft_initialize;
    filter_class->process = ufo_filter_ifft_process;

    /* install properties */
    ifft_properties[PROP_DIMENSIONS] = 
        g_param_spec_int("dimensions",
            "Number of FFT dimensions from 1 to 3",
            "Number of FFT dimensions from 1 to 3",
            1,   /* minimum */
            3,   /* maximum */
            1,   /* default */
            G_PARAM_READWRITE);

    ifft_properties[PROP_SIZE_X] = 
        g_param_spec_int("size-x",
            "Size of the FFT transform in x-direction",
            "Size of the FFT transform in x-direction",
            1,      /* minimum */
            8192,   /* maximum */
            1,      /* default */
            G_PARAM_READWRITE);

    ifft_properties[PROP_SIZE_Y] = 
        g_param_spec_int("size-y",
            "Size of the FFT transform in y-direction",
            "Size of the FFT transform in y-direction",
            1,      /* minimum */
            8192,   /* maximum */
            1,      /* default */
            G_PARAM_READWRITE);

    ifft_properties[PROP_SIZE_Z] = 
        g_param_spec_int("size-z",
            "Size of the FFT transform in z-direction",
            "Size of the FFT transform in z-direction",
            1,      /* minimum */
            8192,   /* maximum */
            1,      /* default */
            G_PARAM_READWRITE);

    ifft_properties[PROP_FINAL_WIDTH] = 
        g_param_spec_int("final-width",
            "Specify if target width is smaller than FFT size",
            "Specify if target width is smaller than FFT size",
            -1,      /* minimum */
            8192,   /* maximum */
            -1,     /* default -1 means "use FFT size" */
            G_PARAM_READWRITE);

    ifft_properties[PROP_FINAL_HEIGHT] = 
        g_param_spec_int("final-height",
            "Specify if target height is smaller than FFT size",
            "Specify if target height is smaller than FFT size",
            -1,      /* minimum */
            8192,   /* maximum */
            -1,     /* default -1 means "use FFT size" */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_DIMENSIONS, ifft_properties[PROP_DIMENSIONS]);
    g_object_class_install_property(gobject_class, PROP_SIZE_X, ifft_properties[PROP_SIZE_X]);
    g_object_class_install_property(gobject_class, PROP_SIZE_Y, ifft_properties[PROP_SIZE_Y]);
    g_object_class_install_property(gobject_class, PROP_SIZE_Z, ifft_properties[PROP_SIZE_Z]);
    g_object_class_install_property(gobject_class, PROP_FINAL_WIDTH, ifft_properties[PROP_FINAL_WIDTH]);
    g_object_class_install_property(gobject_class, PROP_FINAL_HEIGHT, ifft_properties[PROP_FINAL_HEIGHT]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterIFFTPrivate));
}

static void ufo_filter_ifft_init(UfoFilterIFFT *self)
{
    UfoFilterIFFTPrivate *priv = self->priv = UFO_FILTER_IFFT_GET_PRIVATE(self);
    priv->ifft_dimensions = 1;
    priv->ifft_size.x = 1;
    priv->ifft_size.y = 1;
    priv->ifft_size.z = 1;
    priv->final_width = -1;
    priv->final_height = -1;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_IFFT, NULL);
}
