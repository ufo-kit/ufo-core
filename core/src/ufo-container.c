#include "ufo-container.h"
#include "ufo-element.h"

G_DEFINE_TYPE(UfoContainer, ufo_container, UFO_TYPE_ELEMENT);

#define UFO_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONTAINER, UfoContainerPrivate))


struct _UfoContainerPrivate {
    GList *elements;
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
UfoContainer *ufo_container_new()
{
    UfoContainer *container = g_object_new(UFO_TYPE_CONTAINER, NULL);
    return container;
}

void ufo_container_add_element(UfoContainer *container, UfoElement *element)
{
    container->priv->elements = g_list_append(container->priv->elements, element);
}

/* 
 * virtual methods 
 */
static void ufo_container_class_init(UfoContainerClass *klass)
{
    g_type_class_add_private(klass, sizeof(UfoContainerPrivate));
}

static void ufo_container_init(UfoContainer *self)
{
    /* init public fields */

    /* init private fields */
    UfoContainerPrivate *priv;
    self->priv = priv = UFO_CONTAINER_GET_PRIVATE(self);
    priv->elements = NULL;
}
