#include "ufo-filter.h"
#include "ufo-buffer.h"

G_DEFINE_TYPE(UfoFilter, ufo_filter, G_TYPE_OBJECT);

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

struct _UfoFilterPrivate {
};

/* 
 * non-virtual public methods 
 */

/*
 * \brief Set name of this filter
 *
 * \param[in] name Name of the filter
 */
void ufo_filter_set_name(UfoFilter *self, const gchar *name)
{
    self->name = g_strdup(name);
}


/* 
 * virtual methods
 */
void ufo_filter_process(UfoFilter *self, UfoBuffer *input, UfoBuffer *output)
{
    g_return_if_fail(UFO_IS_FILTER(self));
    UFO_FILTER_GET_CLASS(self)->process(self, input, output);
}

static void ufo_filter_process_default(UfoFilter *self, UfoBuffer *input, UfoBuffer *output)
{
    g_return_if_fail(UFO_IS_FILTER(self));
}

static void ufo_filter_finalize(GObject *gobject)
{
    UfoFilter *self = UFO_FILTER(gobject);

    if (self->name != NULL)
        g_free(self->name);
}

static void ufo_filter_class_init(UfoFilterClass *klass)
{
    /* override GObject methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_filter_finalize;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoFilterPrivate));
    klass->process = ufo_filter_process_default;
}

static void ufo_filter_init(UfoFilter *self)
{
    /* init public fields */
    self->name = NULL;

    /* init private fields */
    UfoFilterPrivate *priv;
    self->priv = priv = UFO_FILTER_GET_PRIVATE(self);
}


