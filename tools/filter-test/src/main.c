
#include "ufo-buffer.h"
#include "ufo-filter.h"
#include "ufo-filter-hist.h"

int main(int argc, char *argv[])
{
    g_type_init();

    UfoBuffer *buffer = g_object_new(UFO_TYPE_BUFFER, NULL);
    g_object_ref(buffer);
    
    ufo_buffer_set_dimensions(buffer, 10, 10);
    ufo_buffer_set_bytes_per_pixel(buffer, 1);
    ufo_buffer_malloc(buffer);

    UfoFilter *source= g_object_new(UFO_TYPE_FILTER, NULL);
    g_object_ref(source);

    UfoFilterHist *sink = g_object_new(UFO_TYPE_FILTER_HIST, NULL);
    g_object_ref(sink);

    ufo_filter_set_name(source, "source");
    ufo_filter_set_name(UFO_FILTER(sink), "sink");
    ufo_filter_set_output(source, UFO_FILTER(sink));
    ufo_filter_set_input(UFO_FILTER(sink), source);

    ufo_filter_set_output_buffer(source, buffer);
    ufo_filter_set_input_buffer(UFO_FILTER(sink), buffer);

    ufo_filter_process(source);

    g_object_unref(buffer);
    g_object_unref(source);
    g_object_unref(sink);

    return 0;
}
