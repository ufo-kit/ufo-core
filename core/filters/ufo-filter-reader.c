#include <gmodule.h>
#include <tiffio.h>

#include "ufo-filter-reader.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterReaderPrivate {
    gchar *path;
    gchar *prefix;
    gint count;
    GList *filenames;
};

GType ufo_filter_reader_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterReader, ufo_filter_reader, UFO_TYPE_FILTER);

#define UFO_FILTER_READER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_READER, UfoFilterReaderPrivate))

enum {
    PROP_0,
    PROP_PATH,
    PROP_PREFIX,
    PROP_COUNT,
    N_PROPERTIES
};

static GParamSpec *reader_properties[N_PROPERTIES] = { NULL, };


static gboolean filter_decode_tiff(TIFF *tif, void *buffer, size_t bytes_per_sample)
{
    const int strip_size = TIFFStripSize(tif);
    const int n_strips = TIFFNumberOfStrips(tif);
    int offset = 0;
    int result = 0;

    for (int strip = 0; strip < n_strips; strip++) {
        result = TIFFReadEncodedStrip(tif, strip, buffer+offset, strip_size);
        if (result == -1)
            return FALSE;
        offset += result / bytes_per_sample;
    }
    return TRUE;
}

static void *filter_read_tiff(const gchar *filename, 
    guint16 *bits_per_sample,
    guint16 *samples_per_pixel,
    guint32 *width,
    guint32 *height)
{
    TIFF *tif = TIFFOpen(filename, "r");
    if (tif == NULL)
        return NULL;

    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, height);

    if (*samples_per_pixel != 1)
        goto error_close;

    size_t bytes_per_sample = *bits_per_sample >> 3;
    void *buffer = g_malloc0(bytes_per_sample * (*width) * (*height));

    if (!filter_decode_tiff(tif, buffer, bytes_per_sample))
        goto error_close;

    return buffer;

error_close:
    TIFFClose(tif);
    return NULL;
}

static void filter_dispose_filenames(UfoFilterReaderPrivate *priv)
{
    if (priv->filenames != NULL) {
        g_list_foreach(priv->filenames, (GFunc) g_free, NULL);
        priv->filenames = NULL;
    }
}

static void filter_read_filenames(UfoFilterReaderPrivate *priv)
{
    filter_dispose_filenames(priv);

    GDir *directory = g_dir_open(priv->path, 0, NULL);
    if (directory == NULL) {
        g_debug("Could not open %s", priv->path);
        return;
    }

    gchar *filename = (gchar *) g_dir_read_name(directory);
    while (filename != NULL) {
        gchar *filepath = g_strdup_printf("%s/%s", priv->path, filename);
        priv->filenames = g_list_append(priv->filenames, filepath);
        filename = (gchar *) g_dir_read_name(directory);
    }
    priv->filenames = g_list_sort(priv->filenames, (GCompareFunc) g_strcmp0);
    g_debug("list length = %i", g_list_length(priv->filenames));
}

/* 
 * virtual methods 
 */
static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

static void ufo_filter_reader_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterReader *filter = UFO_FILTER_READER(object);

    switch (property_id) {
        case PROP_PATH:
            g_free(filter->priv->path);
            filter->priv->path = g_strdup(g_value_get_string(value));
            break;
        case PROP_PREFIX:
            g_free(filter->priv->prefix);
            filter->priv->prefix = g_strdup(g_value_get_string(value));
            break;
        case PROP_COUNT:
            filter->priv->count = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_reader_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterReader *filter = UFO_FILTER_READER(object);

    switch (property_id) {
        case PROP_PATH:
            g_value_set_string(value, filter->priv->path);
            break;
        case PROP_PREFIX:
            g_value_set_string(value, filter->priv->prefix);
            break;
        case PROP_COUNT:
            g_value_set_int(value, filter->priv->count);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_reader_dispose(GObject *object)
{
    filter_dispose_filenames(UFO_FILTER_READER_GET_PRIVATE(UFO_FILTER(object)));
    G_OBJECT_CLASS(ufo_filter_reader_parent_class)->dispose(object);
}

static void ufo_filter_reader_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    UfoFilterReaderPrivate *priv = UFO_FILTER_READER_GET_PRIVATE(self);
    UfoResourceManager *manager = ufo_filter_get_resource_manager(self);
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(self));

    filter_read_filenames(priv);
    const guint max_count = (priv->count == -1) ? G_MAXUINT : priv->count;
    guint32 width, height;
    guint16 bits_per_sample, samples_per_pixel;

    GList *filename = g_list_first(priv->filenames);
    for (guint i = 0; i < max_count && filename != NULL; i++) {
        g_debug("Reading file %s", (char *) filename->data);
        void *buffer = filter_read_tiff((char *) filename->data,
                &bits_per_sample, &samples_per_pixel,
                &width, &height);

        if (buffer == NULL)
            /* break out of the loop and insert finishing buffer */
            break;

        g_debug(" bits per sample: %i", bits_per_sample);
        g_debug(" samples per pixel: %i", samples_per_pixel);
        g_debug(" dimension: %ix%i", width, height);

        UfoBuffer *image = ufo_resource_manager_request_buffer(manager, width, height, NULL);
        ufo_buffer_set_cpu_data(image, buffer, width * height, NULL);
        ufo_buffer_reinterpret(image, bits_per_sample, width * height);

        g_async_queue_push(output_queue, image);
        g_free(buffer);
        filename = g_list_next(filename);
    }

    /* No more data */
    g_async_queue_push(output_queue, 
            ufo_resource_manager_request_finish_buffer(manager));
}

static void ufo_filter_reader_class_init(UfoFilterReaderClass *klass)
{
    /* override methods */
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = ufo_filter_reader_set_property;
    gobject_class->get_property = ufo_filter_reader_get_property;
    gobject_class->dispose = ufo_filter_reader_dispose;
    filter_class->process = ufo_filter_reader_process;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;

    /* install properties */
    reader_properties[PROP_PREFIX] = 
        g_param_spec_string("prefix",
            "Filename prefix",
            "Prefix of input filename",
            "",
            G_PARAM_READWRITE);

    reader_properties[PROP_PATH] = 
        g_param_spec_string("path",
            "File path",
            "Path to data files",
            ".",
            G_PARAM_READWRITE);

    reader_properties[PROP_COUNT] =
        g_param_spec_int("count",
        "Number of files",
        "Number of files to read with -1 denoting all",
        -1,   /* minimum */
        4096, /* maximum */
        -1,   /* default */
        G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_PATH, reader_properties[PROP_PATH]);
    g_object_class_install_property(gobject_class, PROP_PREFIX, reader_properties[PROP_PREFIX]);
    g_object_class_install_property(gobject_class, PROP_COUNT, reader_properties[PROP_COUNT]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterReaderPrivate));
}

static void ufo_filter_reader_init(UfoFilterReader *self)
{
    self->priv = UFO_FILTER_READER_GET_PRIVATE(self);
    self->priv->path = g_strdup(".");
    self->priv->prefix = NULL;
    self->priv->count = -1;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_READER, NULL);
}
