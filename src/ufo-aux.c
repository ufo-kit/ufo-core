#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "ufo-aux.h"

void
ufo_set_property_object (GObject **storage, GObject *object)
{
    if (*storage != NULL)
        g_object_unref (*storage);

    *storage = object;

    if (object != NULL)
        g_object_ref (object);
}

void
ufo_unref_stored_object (GObject **object)
{
    if (*object != NULL) {
        g_object_unref (*object);
        *object = NULL;
    }
}

void
ufo_debug_cl (const gchar *format, ...)
{
    va_list args;

    va_start (args, format);
    g_logv ("ocl", G_LOG_LEVEL_MESSAGE, format, args);
    va_end (args);
}


