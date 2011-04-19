#ifndef __UFO_FILTER_H
#define __UFO_FILTER_H

#include <glib-object.h>
#include <ethos/ethos.h>

#include "ufo-resource-manager.h"

#define UFO_TYPE_FILTER             (ufo_filter_get_type())
#define UFO_FILTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER, UfoFilter))
#define UFO_IS_FILTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER))
#define UFO_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER, UfoFilterClass))
#define UFO_IS_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER))
#define UFO_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER, UfoFilterClass))

typedef struct _UfoFilter           UfoFilter;
typedef struct _UfoFilterClass      UfoFilterClass;
typedef struct _UfoFilterPrivate    UfoFilterPrivate;

/**
 * \class UfoFilter
 * \brief Abstract and encapsulated unit of computation
 * \extends EthosPlugin
 */
struct _UfoFilter {
    EthosPlugin parent_instance;

    /* private */
    UfoFilterPrivate *priv;
};

struct _UfoFilterClass {
    EthosPluginClass parent_class;

    void (*process) (UfoFilter *filter);
};

void ufo_filter_process(UfoFilter *filter);

void ufo_filter_set_resource_manager(UfoFilter *filter, UfoResourceManager *resource_manager);
UfoResourceManager *ufo_filter_get_resource_manager(UfoFilter *filter);
void ufo_filter_set_input_queue(UfoFilter *filter, GAsyncQueue *input_queue);
void ufo_filter_set_output_queue(UfoFilter *filter, GAsyncQueue *output_queue);
GAsyncQueue *ufo_filter_get_input_queue(UfoFilter *filter);
GAsyncQueue *ufo_filter_get_output_queue(UfoFilter *filter);

GType ufo_filter_get_type(void);

#endif
