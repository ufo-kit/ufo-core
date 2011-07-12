#ifndef __UFO_FILTER_H
#define __UFO_FILTER_H

#include <glib-object.h>
#include <ethos/ethos.h>

#include "ufo-resource-manager.h"
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

/**
 * \class UfoFilter
 * \brief Abstract and encapsulated unit of computation
 * \extends EthosPlugin
 * \implements UfoElement
 *
 * <b>Signals</b>
 *
 * <b>Properties</b>
 */
struct _UfoFilter {
    EthosPlugin parent_instance;

    /* private */
    UfoFilterPrivate *priv;
};

struct _UfoFilterClass {
    EthosPluginClass parent_class;

    void (*initialize) (UfoFilter *filter);
    void (*process) (UfoFilter *filter);
};

void ufo_filter_initialize(UfoFilter *filter);
void ufo_filter_process(UfoFilter *filter);

UfoBuffer *ufo_filter_pop_buffer(UfoFilter *filter);
void ufo_filter_push_buffer(UfoFilter *filter, UfoBuffer *buffer);
void ufo_filter_account_gpu_time(UfoFilter *filter, void **event);
float ufo_filter_get_gpu_time(UfoFilter *filter);


GType ufo_filter_get_type(void);

#endif
