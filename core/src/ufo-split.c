#include "ufo-split.h"

G_DEFINE_TYPE(UfoSplit, ufo_split, UFO_TYPE_CONTAINER);

#define UFO_SPLIT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SPLIT, UfoSplitPrivate))

enum {
    PROP_0,

    PROP_MODE
};

typedef enum {
    MODE_RANDOM = 0,
    MODE_ROUND_ROBIN,
    MODE_COPY,
    NUM_MODES
} mode_t;

static const char* MODE_STRINGS[] = {
    "random",
    "round-robin",
    "copy"
};

struct _UfoSplitPrivate {
    mode_t mode;
};


/* 
 * Public Interface
 */
UfoContainer *ufo_split_new()
{
    return UFO_CONTAINER(g_object_new(UFO_TYPE_SPLIT, NULL));
}

/* 
 * Virtual Methods 
 */
static void ufo_split_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoSplit *self = UFO_SPLIT(object);

    switch (property_id) {
        case PROP_MODE:
            for (guint i = 0; i < NUM_MODES; i++) {
                if (g_strcmp0(MODE_STRINGS[i], g_value_get_string(value)) == 0) {
                    self->priv->mode = (mode_t) i;
                    break;
                }
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_split_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoSplit *self = UFO_SPLIT(object);

    switch (property_id) {
        case PROP_MODE:
            g_value_set_string(value, MODE_STRINGS[self->priv->mode]);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_split_process(UfoElement *element)
{
}

/*
 * Type/Class Initialization
 */
static void ufo_split_class_init(UfoSplitClass *klass)
{
    /* override methods */
    UfoElementClass *element_class = UFO_ELEMENT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = ufo_split_set_property;
    gobject_class->get_property = ufo_split_get_property;
    element_class->process = ufo_split_process;

    g_type_class_add_private(klass, sizeof(UfoSplitPrivate));
}

static void ufo_split_init(UfoSplit *self)
{
    self->priv = UFO_SPLIT_GET_PRIVATE(self);
}
