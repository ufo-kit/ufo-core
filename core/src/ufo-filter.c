#include "ufo-filter.h"
#include "ufo-buffer.h"

G_DEFINE_TYPE(UfoFilter, ufo_filter, G_TYPE_OBJECT);

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

struct _UfoFilterPrivate {
    UfoFilter *input;
    UfoFilter *output;
    UfoBuffer *input_buffer;
    UfoBuffer *output_buffer;
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
 * \brief Set input filter of this filter
 *
 * If an input filter is not set, the filter acts like a source.
 *
 * \param[in] input Filter from which to receive data
 * \return FALSE if circle is detected
 */
gboolean ufo_filter_set_input(UfoFilter *self, UfoFilter *input)
{
    if (self == input)
        return FALSE;
    self->priv->input = input;
    return TRUE;
}

/*
 * \brief Set output filter of this filter
 *
 * If an output filter is not set, the filter acts like a sink.
 *
 * \param[in] out Filter to which to push data
 * \return FALSE if circle is detected
 */
gboolean ufo_filter_set_output(UfoFilter *self, UfoFilter *output)
{
    if (self == output)
        return FALSE;
    self->priv->output = output;
    return TRUE;
}

/*
 * \brief Set buffer from where to read
 *
 * \param[in] buffer Buffer from which filter reads
 */
void ufo_filter_set_input_buffer(UfoFilter *self, UfoBuffer *buffer)
{
    g_object_ref(buffer);
    self->priv->input_buffer = buffer;
}

UfoBuffer *ufo_filter_get_input_buffer(UfoFilter *self)
{
    return self->priv->input_buffer;
}

/*
 * \brief Set buffer where to write
 *
 * \param[in] buffer Buffer to write
 */
void ufo_filter_set_output_buffer(UfoFilter *self, UfoBuffer *buffer)
{
    g_object_ref(buffer);
    self->priv->output_buffer = buffer;
}

UfoBuffer *ufo_filter_get_output_buffer(UfoFilter *self)
{
    return self->priv->output_buffer;
}

/* 
 * virtual methods
 */
void ufo_filter_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));
    UFO_FILTER_GET_CLASS(self)->process(self);
}

static void ufo_filter_process_default(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));
    if (self->priv->output != NULL)
        ufo_filter_process(self->priv->output);
}

static void ufo_filter_dispose(GObject *gobject)
{
    UfoFilter *self = UFO_FILTER(gobject);
    
    if (self->priv->input) {
        g_object_unref(self->priv->input);
        self->priv->input = NULL;
    }
    if (self->priv->output) {
        g_object_unref(self->priv->output);
        self->priv->output = NULL;
    }

    if (self->priv->input_buffer) {
        g_object_unref(self->priv->input_buffer);
        self->priv->input_buffer = NULL;
    }
    if (self->priv->output_buffer) {
        g_object_unref(self->priv->output_buffer);
        self->priv->output_buffer = NULL;
    }

    G_OBJECT_CLASS(ufo_filter_parent_class)->dispose(gobject);
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
    gobject_class->dispose = ufo_filter_dispose;
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
    priv->input = NULL;
    priv->output = NULL;
    priv->input_buffer = NULL;
    priv->output_buffer = NULL;
}


