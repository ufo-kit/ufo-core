#ifndef __UFO_RELATION_H
#define __UFO_RELATION_H

#include <glib-object.h>
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_RELATION             (ufo_relation_get_type())
#define UFO_RELATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RELATION, UfoRelation))
#define UFO_IS_RELATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RELATION))
#define UFO_RELATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RELATION, UfoRelationClass))
#define UFO_IS_RELATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RELATION))
#define UFO_RELATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RELATION, UfoRelationClass))

typedef struct _UfoRelation           UfoRelation;
typedef struct _UfoRelationClass      UfoRelationClass;
typedef struct _UfoRelationPrivate    UfoRelationPrivate;

typedef enum {
    UFO_RELATION_MODE_DISTRIBUTE,
    UFO_RELATION_MODE_COPY
} UfoRelationMode;

/**
 * UfoRelation:
 *
 * Data transport channel between two #UfoFilter objects. The contents of the
 * #UfoRelation structure are private and should only be accessed via the
 * provided API.
 */
struct _UfoRelation {
    /*< private >*/
    GObject parent_instance;

    UfoRelationPrivate *priv;
};

/**
 * UfoRelationClass:
 *
 * #UfoRelation class
 */
struct _UfoRelationClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoRelation *ufo_relation_new(UfoFilter *producer, guint output_port, UfoRelationMode mode);
UfoFilter *ufo_relation_get_producer(UfoRelation *relation);
GList *ufo_relation_get_consumers(UfoRelation *relation);
void ufo_relation_add_consumer(UfoRelation *relation, UfoFilter *consumer, guint input_port, GError **error);
gboolean ufo_relation_has_consumer(UfoRelation *relation, UfoFilter *consumer);

guint ufo_relation_get_producer_port(UfoRelation *relation);
guint ufo_relation_get_consumer_port(UfoRelation *relation, UfoFilter *consumer);
void ufo_relation_get_producer_queues(UfoRelation *relation, GAsyncQueue **push_queue, GAsyncQueue **pop_queue);
void ufo_relation_get_consumer_queues(UfoRelation *relation, UfoFilter *consumer, GAsyncQueue **push_queue, GAsyncQueue **pop_queue);
void ufo_relation_push_poison_pill(UfoRelation *relation);

GType ufo_relation_get_type(void);

G_END_DECLS

#endif
