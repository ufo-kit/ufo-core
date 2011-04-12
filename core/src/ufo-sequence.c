#include "ufo-sequence.h"

G_DEFINE_TYPE(UfoSequence, ufo_sequence, UFO_TYPE_CONTAINER);

#define UFO_SEQUENCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SEQUENCE, UfoSequencePrivate))


struct _UfoSequencePrivate {
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
UfoSequence *ufo_sequence_new()
{
    UfoSequence *sequence = g_object_new(UFO_TYPE_SEQUENCE, NULL);
    return sequence;
}

/* 
 * virtual methods 
 */
static void ufo_sequence_class_init(UfoSequenceClass *klass)
{
    g_type_class_add_private(klass, sizeof(UfoSequencePrivate));
}

static void ufo_sequence_init(UfoSequence *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_SEQUENCE_GET_PRIVATE(self);
}
