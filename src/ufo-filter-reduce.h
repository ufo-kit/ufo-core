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
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterReduce {
    /*< private >*/
    UfoFilter parent;
};

/**
 * UfoFilterReduceClass:
 * @parent: the parent class
 * @initialize: the @initialize function is called by an UfoBaseScheduler to set
 *      up a filter before actual execution happens. It receives the first input
 *      data to which the filter can get adjust. The reduce filter should return
 *      the default value that is used to fill the output buffer.
 * @collect: the @collect function is called for each new input. The output
 *      buffer will be the same for all invocations and can be used to
 *      accumulate results.
 * @reduce: the @reduce function is called after the data stream end. It is used
 *      to finish any remaining work on the output.
 */
struct _UfoFilterReduceClass {
    UfoFilterClass parent;

    /* overridable */
    void (*initialize)  (UfoFilterReduce    *filter,
                         UfoBuffer          *input[],
                         guint             **output_dims,
                         gfloat             *default_value,
                         GError            **error);
    void (*collect)     (UfoFilterReduce    *filter,
                         UfoBuffer          *input[],
                         UfoBuffer          *output[],
                         GError            **error);
    gboolean (*reduce)  (UfoFilterReduce    *filter,
                         UfoBuffer          *output[],
                         GError            **error);
};

void     ufo_filter_reduce_initialize (UfoFilterReduce *filter,
                                       UfoBuffer       *input[],
                                       guint          **output_dims,
                                       gfloat          *default_value,
                                       GError         **error);
void     ufo_filter_reduce_collect    (UfoFilterReduce *filter,
                                       UfoBuffer       *input[],
                                       UfoBuffer       *output[],
                                       GError         **error);
gboolean ufo_filter_reduce_reduce     (UfoFilterReduce *filter,
                                       UfoBuffer       *output[],
                                       GError         **error);
GType    ufo_filter_reduce_get_type   (void);

G_END_DECLS

#endif
