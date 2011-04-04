
#include <stdio.h>
#include "ufo-buffer.h"
#include "ufo-filter.h"
#include "ufo-filter-hist.h"
#include "ufo-filter-raw-source.h"

int main(int argc, char *argv[])
{
    g_type_init();

    UfoBuffer *sinogram = g_object_new(UFO_TYPE_BUFFER, NULL);
    g_object_ref(sinogram);
    
    ufo_buffer_set_dimensions(sinogram, 1528, 720);
    ufo_buffer_set_bytes_per_pixel(sinogram, sizeof(float));
    ufo_buffer_malloc(sinogram);

    UfoFilterRawSource *source = g_object_new(UFO_TYPE_FILTER_RAW_SOURCE, NULL);
    g_object_ref(source);

    ufo_filter_raw_source_set_info(source, argv[1], 1528, 720, sizeof(float));

    UfoFilterHist *sink = g_object_new(UFO_TYPE_FILTER_HIST, NULL);
    g_object_ref(sink);

    ufo_filter_set_name(UFO_FILTER(source), "source");
    ufo_filter_set_name(UFO_FILTER(sink), "sink");
    ufo_filter_set_output(UFO_FILTER(source), UFO_FILTER(sink));

    ufo_filter_set_output_buffer(UFO_FILTER(source), sinogram);
    ufo_filter_set_input_buffer(UFO_FILTER(sink), sinogram);

    ufo_filter_process(UFO_FILTER(source));

    g_object_unref(sinogram);
    g_object_unref(source);
    g_object_unref(sink);

    return 0;
}
