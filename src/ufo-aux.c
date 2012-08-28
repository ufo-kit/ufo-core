#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "ufo-aux.h"

/**
 * ufo_set_property_object:
 * @storage: A place to store @object
 * @object: An object that should be stored and referenced or %NULL
 *
 * Tries to store @object in @storage and references if @object is not %NULL. In
 * case @storage contains already an object, the existing object is unreffed
 * with g_object_unref().
 */
void
ufo_set_property_object (GObject **storage, GObject *object)
{
    if (*storage != NULL)
        g_object_unref (*storage);

    *storage = object;

    if (object != NULL)
        g_object_ref (object);
}

/**
 * ufo_unref_stored_object:
 * @storage: A place that could contain an object.
 *
 * If @storage contains an object it is unreffed with %NULL and the storage set
 * to %NULL.
 */
void
ufo_unref_stored_object (GObject **storage)
{
    if (*storage != NULL) {
        g_object_unref (*storage);
        *storage = NULL;
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


