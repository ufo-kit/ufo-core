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

void ufo_container_join_threads(gpointer data, gpointer user_data)
{
    GThread *thread = (GThread *) data;
    g_thread_join(thread);
}

/* 
 * Virtual Methods 
 */
/**
 * \brief Add elements successively to the container
 * \public \memberof UfoContainer
 * 
 * The exact way how elements are arranged depends on an actual Container
 * implementation like UfoSplit or UfoContainer
 *
 * \param[in] container A UfoContainer object
 * \param[in] element A UfoElement
 */
void ufo_container_add_element(UfoContainer *container, UfoElement *element)
{
    UFO_CONTAINER_GET_CLASS(container)->add_element(container, element);
}

/**
 * ufo_container_get_elements:
 * 
 * Return value: (element-type UfoElement) (transfer none): list of children.
 */
GList *ufo_container_get_elements(UfoContainer *container)
{
    return UFO_CONTAINER_GET_CLASS(container)->get_elements(container);
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
