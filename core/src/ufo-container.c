#include "ufo-container.h"

G_DEFINE_TYPE(UfoContainer, ufo_container, G_TYPE_OBJECT);

#define UFO_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONTAINER, UfoContainerPrivate))


struct _UfoContainerPrivate {
    int dummy;
};


/* 
 * Public Interface
 */
UfoContainer *ufo_container_new()
{
    return UFO_CONTAINER(g_object_new(UFO_TYPE_CONTAINER, NULL));
}

/* 
 * Virtual Methods 
 */
void ufo_container_add_element(UfoContainer *self, UfoElement *element)
{
    UFO_CONTAINER_GET_CLASS(self)->add_element(self, element);
}

/*
 * Type/Class Initialization
 */
static void ufo_container_class_init(UfoContainerClass *klass)
{
    /* override methods */
    klass->add_element = NULL;
}

static void ufo_container_init(UfoContainer *self)
{
}
