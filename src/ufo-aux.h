#ifndef __UFO_AUX_H
#define __UFO_AUX_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _UfoEventList UfoEventList;

UfoEventList *ufo_event_list_new                (guint          n_events);
void          ufo_event_list_free               (UfoEventList  *list);
gpointer      ufo_event_list_get_event_array    (UfoEventList  *list);
GList        *ufo_event_list_get_list           (UfoEventList  *list);

G_END_DECLS

#endif
