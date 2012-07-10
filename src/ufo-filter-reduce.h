#ifndef __UFO_FILTER_REDUCE_H
#define __UFO_FILTER_REDUCE_H

#include <glib-object.h>

#include "ufo-buffer.h"
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_REDUCE             (ufo_filter_reduce_get_type())
#define UFO_FILTER_REDUCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_REDUCE, UfoFilterReduce))
#define UFO_IS_FILTER_REDUCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_REDUCE))
#define UFO_FILTER_REDUCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_REDUCE, UfoFilterReduceClass))
#define UFO_IS_FILTER_REDUCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_REDUCE))
#define UFO_FILTER_REDUCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_REDUCE, UfoFilterReduceClass))

typedef struct _UfoFilterReduce           UfoFilterReduce;
typedef struct _UfoFilterReduceClass      UfoFilterReduceClass;
typedef struct _UfoFilterReducePrivate    UfoFilterReducePrivate;

/**
 * UfoFilterReduce:
 *
 * Creates #UfoFilterReduce instances by loading corresponding shared objects. The
 * contents of the #UfoFilterReduce structure are private and should only be
 * accessed via the provided API.
 */
struct _UfoFilterReduce {
    /*< private >*/
    UfoFilter parent;

    UfoFilterReducePrivate *priv;
};

/**
 * UfoFilterReduceClass:
 *
 * #UfoFilterReduce class
 */
struct _UfoFilterReduceClass {
    /*< private >*/
    UfoFilterClass parent;

    void (*initialize)  (UfoFilterReduce *filter, UfoBuffer *input[], guint **output_dims, GError **error);
    void (*collect)     (UfoFilterReduce *filter, UfoBuffer *input[], gpointer cmd_queue, GError **error);
    void (*reduce)      (UfoFilterReduce *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error);
};

void  ufo_filter_reduce_initialize (UfoFilterReduce *filter,
                                    UfoBuffer       *input[],
                                    guint          **output_dims,
                                    GError         **error);
void  ufo_filter_reduce_collect    (UfoFilterReduce *filter,
                                    UfoBuffer       *input[],
                                    gpointer         cmd_queue,
                                    GError         **error);
void  ufo_filter_reduce_reduce     (UfoFilterReduce *filter,
                                    UfoBuffer       *output[],
                                    gpointer         cmd_queue,
                                    GError         **error);
GType ufo_filter_reduce_get_type   (void);

G_END_DECLS

#endif
