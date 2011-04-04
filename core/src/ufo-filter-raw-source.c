#include <stdio.h>
#include "ufo-filter-raw-source.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterRawSource, ufo_filter_raw_source, UFO_TYPE_FILTER);

#define UFO_FILTER_RAW_SOURCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_RAW_SOURCE, UfoFilterRawSourcePrivate))

struct _UfoFilterRawSourcePrivate {
    gchar  *filename;
    gint32 width;
    gint32 height;
    gint32 bytes_per_pixel;
};


void ufo_filter_raw_source_set_info(UfoFilterRawSource *self, const gchar *filename, gint32 width, gint32 height, gint32 bpp)
{
    UfoFilterRawSourcePrivate *priv = UFO_FILTER_RAW_SOURCE_GET_PRIVATE(self);
    priv->filename = g_strdup(filename); 
    priv->width = width;
    priv->height = height;
    priv->bytes_per_pixel = bpp;
}

/* 
 * virtual methods 
 */
static void ufo_filter_raw_source_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    UfoBuffer *buffer = ufo_filter_get_output_buffer(self);
    if (buffer != NULL) {
        gint32 width, height;
        ufo_buffer_get_dimensions(buffer, &width, &height);


        gint32 bytes_per_pixel = ufo_buffer_get_bytes_per_pixel(buffer);
        UfoFilterRawSourcePrivate *priv = UFO_FILTER_RAW_SOURCE_GET_PRIVATE(self);

        /* check if buffer format is the same as raw format */
        if ((priv->width != width) || (priv->height != height) || (priv->bytes_per_pixel != bytes_per_pixel))
            return;

        gchar* data = ufo_buffer_get_raw_bytes(buffer);
        if (data != NULL) {
            /* TODO: good concept for errors */
            /* FIXME: find appropriate glib calls */
            FILE *fp = fopen(priv->filename, "rb");
            fread(data, bytes_per_pixel, width*height, fp);
            fclose(fp);
        }
        else
            return;
    }

    /* call parent */
    UfoFilterRawSourceClass *klass = UFO_FILTER_RAW_SOURCE_GET_CLASS(self);
    UfoFilterClass *parent_class = g_type_class_peek_parent(klass);
    parent_class->process(UFO_FILTER(self));
}

static void ufo_filter_raw_source_class_init(UfoFilterRawSourceClass *klass)
{
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    filter_class->process = ufo_filter_raw_source_process;

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterRawSourcePrivate));
}

static void ufo_filter_raw_source_init(UfoFilterRawSource *self)
{
    /* init public fields */

    /* init private fields */
    UfoFilterRawSourcePrivate *priv = UFO_FILTER_RAW_SOURCE_GET_PRIVATE(self);
    priv->filename = NULL;
    priv->bytes_per_pixel = 1;
}

