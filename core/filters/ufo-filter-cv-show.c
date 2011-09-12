#include <gmodule.h>
#include <cv.h>
#include <highgui.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-cv-show.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"

struct _UfoFilterCvShowPrivate {
    gboolean show_histogram;
};

GType ufo_filter_cv_show_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterCvShow, ufo_filter_cv_show, UFO_TYPE_FILTER);

#define UFO_FILTER_CV_SHOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_CV_SHOW, UfoFilterCvShowPrivate))

enum {
    PROP_0,
    PROP_SHOW_HISTOGRAM, 
    N_PROPERTIES
};

static GParamSpec *cv_show_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_cv_show_initialize(UfoFilter *filter)
{
}

static IplImage *draw_histogram(CvHistogram *hist, float scale_x, float scale_y)
{
    float hist_max = 0.0;
    cvGetMinMaxHistValue(hist, 0, &hist_max, 0, 0);
    IplImage *img = cvCreateImage(cvSize(256*scale_x, 64*scale_y), 8, 1);
    cvZero(img);
    for (int i = 0; i < 255; i++) {
        float hist_value = cvQueryHistValue_1D(hist, i); 
        float next_value = cvQueryHistValue_1D(hist, i+1); 
        CvPoint p1 = cvPoint(i*scale_x, 64*scale_y);
        CvPoint p2 = cvPoint(i*scale_x+scale_x, 64*scale_y);
        CvPoint p3 = cvPoint(i*scale_x+scale_x, (64-next_value*64/hist_max)*scale_y);
        CvPoint p4 = cvPoint(i*scale_x, (64-hist_value*64/hist_max)*scale_y);
        int num_points = 5;
        CvPoint points[] = {p1, p2, p3, p4, p1};
        cvFillConvexPoly(img, points, num_points, cvScalar(255, 0, 0, 0), 8, 0);
    }
    return img;
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_cv_show_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoChannel *input_channel = ufo_filter_get_input_channel(filter);
    UfoChannel *output_channel = ufo_filter_get_output_channel(filter);
    cl_command_queue command_queue = (cl_command_queue) ufo_filter_get_command_queue(filter);
    UfoFilterCvShow *self = UFO_FILTER_CV_SHOW(filter);
    UfoFilterCvShowPrivate *priv = UFO_FILTER_CV_SHOW_GET_PRIVATE(self);

    CvSize size;
    UfoBuffer *input = ufo_channel_pop(input_channel);
    ufo_buffer_get_2d_dimensions(input, &size.width, &size.height);

    IplImage *image = cvCreateImageHeader(size, IPL_DEPTH_32F, 1);
    IplImage *blit = cvCreateImage(size, IPL_DEPTH_8U, 1);

    gchar *window_name = g_strdup_printf("Foo-%p", filter);
    cvNamedWindow(window_name, CV_WINDOW_AUTOSIZE);
    cvMoveWindow(window_name, 100, 100);

    int num_bins = 256;
    float range[] = {0, 255};
    float *ranges[] = { range };
    CvHistogram *hist = cvCreateHist(1, &num_bins, CV_HIST_ARRAY, ranges, 1);

    while (input != NULL) {
        image->imageData = (char *) ufo_buffer_get_cpu_data(input, command_queue);

        cvConvertImage(image, blit, 0);
        cvShowImage(window_name, image);

        if (priv->show_histogram) {
            cvCalcHist(&blit, hist, 0, 0);
            IplImage *img_hist = draw_histogram(hist, 1.0, 1.0);
            cvClearHist(hist);
            cvShowImage("Histogram", img_hist);
        }
        cvWaitKey(30);
        
        ufo_channel_push(output_channel, input);
        input = ufo_channel_pop(input_channel);
    }
    cvWaitKey(10000);
    cvDestroyWindow(window_name);
    g_free(window_name);

    ufo_channel_finish(output_channel);
}

static void ufo_filter_cv_show_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterCvShow *self = UFO_FILTER_CV_SHOW(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_SHOW_HISTOGRAM:
            self->priv->show_histogram = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_cv_show_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterCvShow *self = UFO_FILTER_CV_SHOW(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_SHOW_HISTOGRAM:
            g_value_set_boolean(value, self->priv->show_histogram);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_cv_show_class_init(UfoFilterCvShowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_cv_show_set_property;
    gobject_class->get_property = ufo_filter_cv_show_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_cv_show_initialize;
    filter_class->process = ufo_filter_cv_show_process;

    /* install properties */
    cv_show_properties[PROP_SHOW_HISTOGRAM] = 
        g_param_spec_boolean("show-histogram",
            "Show also the histogram of the buffer",
            "Show also the histogram of the buffer",
            FALSE,
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_SHOW_HISTOGRAM, cv_show_properties[PROP_SHOW_HISTOGRAM]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterCvShowPrivate));
}

static void ufo_filter_cv_show_init(UfoFilterCvShow *self)
{
    UfoFilterCvShowPrivate *priv = self->priv = UFO_FILTER_CV_SHOW_GET_PRIVATE(self);
    priv->show_histogram = FALSE;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_CV_SHOW, NULL);
}
