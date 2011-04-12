#include "ufo-split.h"

G_DEFINE_TYPE(UfoSplit, ufo_split, UFO_TYPE_CONTAINER);

#define UFO_SPLIT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SPLIT, UfoSplitPrivate))


struct _UfoSplitPrivate {
    int dummy;
};


/* 
 * non-virtual public methods 
 */

UfoContainer *ufo_split_new()
{
    return g_object_new(UFO_TYPE_SPLIT, NULL);
}

/* 
 * virtual methods 
 */
static void ufo_split_class_init(UfoSplitClass *klass)
{
    g_type_class_add_private(klass, sizeof(UfoSplitPrivate));
}

static void ufo_split_init(UfoSplit *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_SPLIT_GET_PRIVATE(self);
}
