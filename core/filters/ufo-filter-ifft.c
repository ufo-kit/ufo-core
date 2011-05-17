#include <gmodule.h>
#include <CL/cl.h>
#include <clFFT.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-ifft.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterIFFTPrivate {
    /* add your private data here */
    /* cl_kernel kernel; */
    float example;
    clFFT_Dimension ifft_dimensions;
    clFFT_Dim3 ifft_size;
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
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    int err = CL_SUCCESS;
    clFFT_Plan ifft_plan = clFFT_CreatePlan(
            (cl_context) ufo_resource_manager_get_context(manager),
            priv->ifft_size, priv->ifft_dimensions,
            clFFT_InterleavedComplexFormat, &err);

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    gint32 width, height;
    ufo_buffer_get_dimensions(input, &width, &height);

    while (!ufo_buffer_is_finished(input)) {
        cl_mem buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(input);
        cl_event event;

        /* FIXME: add spreading */
        /* FIXME: height may not be correct for applications other than FBP */
        /* FIXME: we should wait for previous computations */
        clFFT_ExecuteInterleaved(ufo_buffer_get_command_queue(input),
                ifft_plan, height, clFFT_Inverse, buffer_mem, buffer_mem,
                0, NULL, &event);

        ufo_buffer_wait_on_event(input, event);

        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
    g_async_queue_push(output_queue, input);
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

    g_object_class_install_property(gobject_class, PROP_DIMENSIONS, ifft_properties[PROP_DIMENSIONS]);
    g_object_class_install_property(gobject_class, PROP_SIZE_X, ifft_properties[PROP_SIZE_X]);
    g_object_class_install_property(gobject_class, PROP_SIZE_Y, ifft_properties[PROP_SIZE_Y]);
    g_object_class_install_property(gobject_class, PROP_SIZE_Z, ifft_properties[PROP_SIZE_Z]);

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
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_IFFT, NULL);
}
