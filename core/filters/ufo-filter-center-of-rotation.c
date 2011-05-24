#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-center-of-rotation.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterCenterOfRotationPrivate {
    /* add your private data here */
    /* cl_kernel kernel; */
    float angle_step;
};

GType ufo_filter_center_of_rotation_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterCenterOfRotation, ufo_filter_center_of_rotation, UFO_TYPE_FILTER);

#define UFO_FILTER_CENTER_OF_ROTATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_CENTER_OF_ROTATION, UfoFilterCenterOfRotationPrivate))

enum {
    PROP_0,
    PROP_ANGLE_STEP, /* remove this or add more */
    N_PROPERTIES
};

static GParamSpec *center_of_rotation_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_center_of_rotation_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
    /*
    UfoFilterCenterOfRotation *self = UFO_FILTER_CENTER_OF_ROTATION(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "foo-kernel-file.cl", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "foo-kernel", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
    */
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_center_of_rotation_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterCenterOfRotationPrivate *priv = UFO_FILTER_CENTER_OF_ROTATION_GET_PRIVATE(filter);
    /*UfoResourceManager *manager = ufo_resource_manager();*/
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));
    UfoBuffer *input = NULL;

    /* Calculate the principial horizontal displacement according to "Image
     * processing pipeline for synchrotron-radiation-based tomographic
     * microscopy" by C. Hintermüller et al. (2010, International Union of
     * Crystallography, Singapore) */

    input = (UfoBuffer *) g_async_queue_pop(input_queue);
    float *proj_0 = ufo_buffer_get_cpu_data(input);
    float *proj_180 = NULL;
    int counter = 0;

    /* Take all buffers until we got the opposite projection */
    input = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input)) {
        if (abs((counter++ * priv->angle_step) - 180.0f) < 0.001f) {
            proj_180 = ufo_buffer_get_cpu_data(input); 
            break;
        }
        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }

    gint32 width, height;
    ufo_buffer_get_dimensions(input, &width, &height);

    /* We have basically two parameters for tuning the performance: decreasing
     * max_displacement and not considering the whole images but just some of
     * the lines */
    const gint max_displacement = width / 2;
    const gsize N = max_displacement * 2 - 1;
    float *scores = g_malloc0(N * sizeof(float));
    float *grad = g_malloc0(N * sizeof(float));

    for (int displacement = (-max_displacement + 1); displacement < 0; displacement++) {
        const int index = displacement + max_displacement - 1;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width-abs(displacement); x++) {
                float diff = proj_0[y*width+x] - proj_180[y*width + (width-abs(displacement) - x + 1)];    
                scores[index] += diff * diff;
            }
        }
    }

    for (int displacement = 0; displacement < max_displacement; displacement++) {
        const int index = displacement + max_displacement - 1; 
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width-displacement; x++) {
                float diff = proj_0[y*width+x+displacement] - proj_180[y*width + (width-x+1)];    
                scores[index] += diff * diff;
            }
        }
    }
    grad[0] = 0.0;
    for (int i = 1; i < N; i++)
        grad[i] = scores[i] - scores[i-1];

    /*for (int i = 0; i < N; i++) {*/
        /*int displacement = i - max_displacement + 1;*/
        /*g_print("%i %f %f\n", (width + displacement) / 2, scores[i], grad[i]);*/
    /*}*/

    /* Find local minima. Actually, if max_displacement is not to large (like
     * width/2) the global maximum is always the correct maximum. */
    for (int i = 1; i < N; i++) 
        if (grad[i-1] < 0.0 && grad[i] > 0.0) 
            g_message("Local minimum at %f: %f", (width + i - max_displacement + 1) / 2.0, scores[i]);

    g_free(grad);
    g_free(scores);

    /* Push the 180° projection */
    g_async_queue_push(output_queue, input);

    /* Push any following projections */
    do {
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
        g_async_queue_push(output_queue, input);
    }
    while (!ufo_buffer_is_finished(input));
}

static void ufo_filter_center_of_rotation_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterCenterOfRotation *self = UFO_FILTER_CENTER_OF_ROTATION(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_ANGLE_STEP:
            self->priv->angle_step = g_value_get_double(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_center_of_rotation_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterCenterOfRotation *self = UFO_FILTER_CENTER_OF_ROTATION(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_ANGLE_STEP:
            g_value_set_double(value, self->priv->angle_step);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_center_of_rotation_class_init(UfoFilterCenterOfRotationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_center_of_rotation_set_property;
    gobject_class->get_property = ufo_filter_center_of_rotation_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_center_of_rotation_initialize;
    filter_class->process = ufo_filter_center_of_rotation_process;

    /* install properties */
    center_of_rotation_properties[PROP_ANGLE_STEP] = 
        g_param_spec_double("angle-step",
            "Step between two successive projections",
            "Step between two successive projections",
            0.00001,   /* minimum */
            180.0,   /* maximum */
            1.0,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_ANGLE_STEP, center_of_rotation_properties[PROP_ANGLE_STEP]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterCenterOfRotationPrivate));
}

static void ufo_filter_center_of_rotation_init(UfoFilterCenterOfRotation *self)
{
    UfoFilterCenterOfRotationPrivate *priv = self->priv = UFO_FILTER_CENTER_OF_ROTATION_GET_PRIVATE(self);
    priv->angle_step = 1.0;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_CENTER_OF_ROTATION, NULL);
}
