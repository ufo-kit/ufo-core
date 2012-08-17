#ifndef __UFO_AUX_H
#define __UFO_AUX_H

#include <glib-object.h>

G_BEGIN_DECLS

void ufo_set_property_object (GObject      **storage,
                                                 GObject       *object);
void ufo_unref_stored_object (GObject      **object);
void ufo_debug_cl            (const gchar   *format,
                              ...);

G_END_DECLS

#endif
