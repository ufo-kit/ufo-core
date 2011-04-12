#include "ufo-split.h"

G_DEFINE_TYPE(UfoSplit, ufo_split, UFO_TYPE_CONTAINER);

#define UFO_SPLIT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SPLIT, UfoSplitPrivate))


struct _UfoSplitPrivate {
};


/* 
 * non-virtual public methods 
 */

/*
 * \brief Create a new buffer with given dimensions
 *
 * \param[in] width Width of the two-dimensional buffer
 * \param[in] height Height of the two-dimensional buffer
 * \param[in] bytes_per_pixel Number of bytes per pixel
 *
 * \return Buffer with allocated memory
 */
UfoSplit *ufo_split_new()
{
    UfoSplit *split = g_object_new(UFO_TYPE_SPLIT, NULL);
    return split;
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
