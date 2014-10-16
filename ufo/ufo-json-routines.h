#ifndef UFO_JSON_ROUTINES_H
#define UFO_JSON_ROUTINES_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>
#include <ufo/ufo-plugin-manager.h>

gchar *  ufo_transform_string (const gchar 	*pattern,
                               const gchar 	*s,
                               const gchar 	*separator);

gpointer ufo_object_from_json (JsonObject      	*object,
                               UfoPluginManager	*manager);

#endif
