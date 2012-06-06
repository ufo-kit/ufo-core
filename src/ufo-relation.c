/**
 * SECTION:ufo-relation
 * @Short_description: Data transport between two UfoFilters
 * @Title: UfoRelation
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-relation.h"
#include "ufo-enums.h"

G_DEFINE_TYPE(UfoRelation, ufo_relation, G_TYPE_OBJECT)

#define UFO_RELATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RELATION, UfoRelationPrivate))

struct _UfoRelationPrivate {
    UfoRelationMode mode;

    UfoFilter *producer;
    guint output_port;
    GAsyncQueue *producer_pop_queue;
    GAsyncQueue *producer_push_queue;

    GList *consumers;                   /* contains UfoFilter * */
    GHashTable *consumer_ports;         /* maps from UfoFilter* to input port */
    GHashTable *consumer_pop_queues;    /* maps from UfoFilter* to GAsyncQueue */
    GHashTable *consumer_push_queues;   /* maps from UfoFilter* to GAsyncQueue */
};

enum {
    PROP_0,
    PROP_PRODUCER,
    PROP_OUTPUT_PORT,
    PROP_MODE,
    N_PROPERTIES
};

static GParamSpec *relation_properties[N_PROPERTIES] = { NULL, };

/**
 * ufo_relation_new:
 * @producer: A #UfoFilter
 * @output_port: The outgoing port number of the producer
 * @mode: The relation mode that determines how data is forwarded
 *
 * Creates a new #UfoRelation that represents a 1:m relationship between a
 * @producer and an arbitrary number of consumers..
 *
 * Return value: A new #UfoRelation
 */
UfoRelation *ufo_relation_new(UfoFilter *producer, guint output_port, UfoRelationMode mode)
{
    return UFO_RELATION(g_object_new(UFO_TYPE_RELATION, 
            "producer", producer, 
            "output-port", output_port, 
            "mode", mode, 
            NULL));
}

UfoFilter *ufo_relation_get_producer(UfoRelation *relation)
{
    g_return_val_if_fail(UFO_IS_RELATION(relation), NULL);
    return relation->priv->producer;
}

GList *ufo_relation_get_consumers(UfoRelation *relation)
{
    g_return_val_if_fail(UFO_IS_RELATION(relation), NULL);
    return relation->priv->consumers;
}

void ufo_relation_add_consumer(UfoRelation *relation, UfoFilter *consumer, guint input_port, GError **error)
{
    g_return_if_fail(UFO_IS_RELATION(relation));
    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(relation);

    /* TODO: check that outputs and inputs match */
    g_hash_table_insert(priv->consumer_ports, consumer, GINT_TO_POINTER(input_port));

    if (priv->producer_pop_queue == NULL)
        priv->producer_pop_queue = g_async_queue_new();

    if (priv->producer_push_queue == NULL)
        priv->producer_push_queue = g_async_queue_new();

    if (priv->mode == UFO_RELATION_MODE_DISTRIBUTE) {
        /* 
         * In distribute mode, each consumer consumes from the same input queue and
         * has to push it back to the same used output queue.
         */
        g_hash_table_insert(priv->consumer_push_queues, consumer, priv->producer_pop_queue);
        g_hash_table_insert(priv->consumer_pop_queues, consumer, priv->producer_push_queue);
    }
    else {
        /*
         * In copy mode, we daisy chain pop-push-queues and transfer the
         * produced buffer from one consumer to the next until it is placed back
         * again in the producers pop queue.
         */
        GList *last_element = g_list_last(priv->consumers);

        if (last_element == NULL) {
            g_hash_table_insert(priv->consumer_push_queues, consumer, priv->producer_pop_queue);
            g_hash_table_insert(priv->consumer_pop_queues, consumer, priv->producer_push_queue);
        }
        else {
            UfoFilter *last_consumer = UFO_FILTER(last_element->data);
            GAsyncQueue *queue = g_async_queue_new(); 
            g_hash_table_insert(priv->consumer_push_queues, last_consumer, queue);
            g_hash_table_insert(priv->consumer_pop_queues, consumer, queue);
            g_hash_table_insert(priv->consumer_push_queues, consumer, priv->producer_pop_queue);
        }
    }

    priv->consumers = g_list_append(priv->consumers, consumer);
}

guint ufo_relation_get_producer_port(UfoRelation *relation)
{
    g_return_val_if_fail(UFO_IS_RELATION(relation), 0);
    return relation->priv->output_port;
}

guint ufo_relation_get_consumer_port(UfoRelation *relation, UfoFilter *consumer)
{
    return (guint) GPOINTER_TO_INT(g_hash_table_lookup(relation->priv->consumer_ports, consumer));
}

void ufo_relation_get_producer_queues(UfoRelation *relation, GAsyncQueue **push_queue, GAsyncQueue **pop_queue)
{
    g_return_if_fail(UFO_IS_RELATION(relation));
    g_return_if_fail(push_queue != NULL && pop_queue != NULL);

    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(relation);
    *push_queue = priv->producer_push_queue;
    *pop_queue = priv->producer_pop_queue;
}

void ufo_relation_get_consumer_queues(UfoRelation *relation, 
        UfoFilter *consumer, GAsyncQueue **push_queue, GAsyncQueue **pop_queue)
{
    g_return_if_fail(UFO_IS_RELATION(relation) && UFO_IS_FILTER(consumer));
    g_return_if_fail(push_queue != NULL && pop_queue != NULL);

    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(relation);
    *push_queue = g_hash_table_lookup(priv->consumer_push_queues, consumer); 
    *pop_queue = g_hash_table_lookup(priv->consumer_pop_queues, consumer);
}

gboolean ufo_relation_has_consumer(UfoRelation *relation, UfoFilter *consumer)
{
    g_return_val_if_fail(UFO_IS_RELATION(relation), FALSE);
    return g_hash_table_lookup_extended(relation->priv->consumer_ports, consumer, NULL, NULL);
}

void ufo_relation_push_poison_pill(UfoRelation *relation)
{
    g_return_if_fail(UFO_IS_RELATION(relation));

    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(relation);

    /*
     * The mode doesn't matter here. If it is distributed, pop_queue will be the
     * same for each consumer thus pushing the pill n times. If copy mode is
     * enable, the pill will be inserted into each consumer's pop queue.
     */
    for (GList *it = g_list_first(priv->consumers); it != NULL; it = g_list_next(it)) {
        UfoFilter *consumer = UFO_FILTER(it->data);
        GAsyncQueue *pop_queue = g_hash_table_lookup(priv->consumer_pop_queues, consumer);
        g_async_queue_push(pop_queue, GINT_TO_POINTER(1));
    }
}

static void ufo_relation_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_PRODUCER:
            priv->producer = g_value_get_object(value);
            break;
        case PROP_OUTPUT_PORT:
            priv->output_port = g_value_get_uint(value);
            break;
        case PROP_MODE:
            priv->mode = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_relation_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_PRODUCER:
            g_value_set_object(value, priv->producer);
            break;
        case PROP_OUTPUT_PORT:
            g_value_set_uint(value, priv->output_port);
            break;
        case PROP_MODE:
            g_value_set_enum(value, priv->mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_relation_dispose(GObject *object)
{
    G_OBJECT_CLASS(ufo_relation_parent_class)->dispose(object);
}

static void ufo_relation_finalize(GObject *object)
{
    UfoRelationPrivate *priv = UFO_RELATION_GET_PRIVATE(object);

    g_list_free(priv->consumers);
    g_hash_table_destroy(priv->consumer_push_queues);
    g_hash_table_destroy(priv->consumer_pop_queues);
    g_hash_table_destroy(priv->consumer_ports);

    G_OBJECT_CLASS(ufo_relation_parent_class)->finalize(object);
}

static void ufo_relation_class_init(UfoRelationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->get_property = ufo_relation_get_property;
    gobject_class->set_property = ufo_relation_set_property;
    gobject_class->dispose      = ufo_relation_dispose;
    gobject_class->finalize     = ufo_relation_finalize;

    relation_properties[PROP_PRODUCER] =
        g_param_spec_object("producer",
                "An UfoFilter",
                "An UfoFilter",
                UFO_TYPE_FILTER,
                G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    relation_properties[PROP_OUTPUT_PORT] =
        g_param_spec_uint("output-port",
                "Number of the producer output port",
                "Number of the producer output port",
                0, 256, 0,
                G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    relation_properties[PROP_MODE] =
        g_param_spec_enum("mode",
                "Work item mode",
                "Work item mode",
                UFO_TYPE_RELATION_MODE, UFO_RELATION_MODE_DISTRIBUTE,
                G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);


    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property(gobject_class, i, relation_properties[i]);

    g_type_class_add_private(klass, sizeof(UfoRelationPrivate));
}

static void ufo_relation_init(UfoRelation *relation)
{
    UfoRelationPrivate *priv;
    relation->priv = priv = UFO_RELATION_GET_PRIVATE(relation);
    priv->producer = NULL;
    priv->output_port = 0;
    priv->consumers = NULL;
    priv->consumer_ports = g_hash_table_new(g_direct_hash, g_direct_equal);

    priv->producer_push_queue= NULL;
    priv->producer_pop_queue = NULL;
    priv->consumer_pop_queues = g_hash_table_new(g_direct_hash, g_direct_equal);
    priv->consumer_push_queues = g_hash_table_new(g_direct_hash, g_direct_equal);
}
