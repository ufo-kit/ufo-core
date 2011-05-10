#include <gmodule.h>
#include <tiffio.h>

#include "ufo-filter-writer.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterWriterPrivate {
    gchar *path;
    gchar *prefix;
};

GType ufo_filter_writer_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterWriter, ufo_filter_writer, UFO_TYPE_FILTER);

#define UFO_FILTER_WRITER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_WRITER, UfoFilterWriterPrivate))

enum {
    PROP_0,
    PROP_PATH,
    PROP_PREFIX,
    N_PROPERTIES
};

static GParamSpec *reader_properties[N_PROPERTIES] = { NULL, };


static gboolean filter_write_tiff(float *buffer, 
    const gchar *name, 
    gint32 width, 
    gint32 height)
{
    gboolean success = TRUE;
    TIFF *tif = TIFFOpen(name, "w");

    if (tif == NULL)
        return FALSE;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 32);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);

    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    size_t written_bytes = TIFFWriteEncodedStrip(tif, 0, buffer, sizeof(short)*width*height);
    if (written_bytes == -1)
        success = FALSE;

    TIFFClose(tif);
    return success;
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

static void ufo_filter_writer_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterWriter *filter = UFO_FILTER_WRITER(object);

    switch (property_id) {
        case PROP_PATH:
            g_free(filter->priv->path);
            filter->priv->path = g_strdup(g_value_get_string(value));
            break;
        case PROP_PREFIX:
            g_free(filter->priv->prefix);
            filter->priv->prefix = g_strdup(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_writer_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterWriter *filter = UFO_FILTER_WRITER(object);

    switch (property_id) {
        case PROP_PATH:
            g_value_set_string(value, filter->priv->path);
            break;
        case PROP_PREFIX:
            g_value_set_string(value, filter->priv->prefix);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_writer_dispose(GObject *object)
{
    G_OBJECT_CLASS(ufo_filter_writer_parent_class)->dispose(object);
}

static void ufo_filter_writer_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    UfoFilterWriterPrivate *priv = UFO_FILTER_WRITER_GET_PRIVATE(self);
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(self));
    UfoBuffer *input = UFO_BUFFER(g_async_queue_pop(input_queue));
    int current_frame = 0;
    GString *filename = g_string_new("");

    while (!ufo_buffer_is_finished(input)) {
        gint32 width, height;
        ufo_buffer_get_dimensions(input, &width, &height);
        float *data = ufo_buffer_get_cpu_data(input);
        g_string_printf(filename, "%s/%s-%i.tif", priv->path, priv->prefix, current_frame++);
        g_message("writing file %s", filename->str);
        if (!filter_write_tiff(data, filename->str, width, height))
            g_message("something went wrong");

        ufo_resource_manager_release_buffer(ufo_resource_manager(), input);
        input = UFO_BUFFER(g_async_queue_pop(input_queue));
    }
}

static void ufo_filter_writer_class_init(UfoFilterWriterClass *klass)
{
    /* override methods */
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = ufo_filter_writer_set_property;
    gobject_class->get_property = ufo_filter_writer_get_property;
    gobject_class->dispose = ufo_filter_writer_dispose;
    filter_class->process = ufo_filter_writer_process;
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
            "Path where to store files",
            ".",
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_PATH, reader_properties[PROP_PATH]);
    g_object_class_install_property(gobject_class, PROP_PREFIX, reader_properties[PROP_PREFIX]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterWriterPrivate));
}

static void ufo_filter_writer_init(UfoFilterWriter *self)
{
    self->priv = UFO_FILTER_WRITER_GET_PRIVATE(self);
    self->priv->path = g_strdup(".");
    self->priv->prefix = NULL;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_WRITER, NULL);
}
