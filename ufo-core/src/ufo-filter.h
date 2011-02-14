#ifndef __UFO_FILTER_H
#define __UFO_FILTER_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-buffer.h"

#define UFO_TYPE_FILTER             (ufo_filter_get_type())
#define UFO_FILTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER, UfoFilter))
#define UFO_IS_FILTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER))
#define UFO_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER, UfoFilterClass))
#define UFO_IS_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER))
#define UFO_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER, UfoFilterClass))

typedef struct _UfoFilter           UfoFilter;
typedef struct _UfoFilterClass      UfoFilterClass;
typedef struct _UfoFilterPrivate    UfoFilterPrivate;

struct _UfoFilter {
    GObject parent_instance;

    /* public */
    gchar *name;

    /* private */
    UfoFilterPrivate *priv;
};

struct _UfoFilterClass {
    GObjectClass parent_class;

    /* class members */

    /* virtual public methods */
    void (*process) (UfoFilter *self);
};

/* virtual public methods */
void ufo_filter_process(UfoFilter *self);
//void ufo_filter_process_default(UfoFilter *self);

/* non-virtual public methods */
void ufo_filter_set_name(UfoFilter *self, const gchar *name);
gboolean ufo_filter_set_input(UfoFilter *self, UfoFilter *input);
gboolean ufo_filter_set_output(UfoFilter *self, UfoFilter *output);
void ufo_filter_set_input_buffer(UfoFilter *self, UfoBuffer *buffer);
void ufo_filter_set_output_buffer(UfoFilter *self, UfoBuffer *buffer);
UfoBuffer *ufo_filter_get_input_buffer(UfoFilter *self);
UfoBuffer *ufo_filter_get_output_buffer(UfoFilter *self);

GType ufo_filter_get_type(void);

#endif
