#ifndef __UFO_FILTER_H
#define __UFO_FILTER_H

#include <glib-object.h>

#include "ufo-profiler.h"
#include "ufo-resource-manager.h"
#include "ufo-aux.h"
#include "ufo-buffer.h"
#include "ufo-channel.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER             (ufo_filter_get_type())
#define UFO_FILTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER, UfoFilter))
#define UFO_IS_FILTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER))
#define UFO_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER, UfoFilterClass))
#define UFO_IS_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER))
#define UFO_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER, UfoFilterClass))

/**
 * UFO_FILTER_INFINITE_INPUT:
 *
 * Use this to specify that you accept an infinite data stream on an input for
 * #UfoInputParameter and ufo_filter_register_inputs().
 */
#define UFO_FILTER_INFINITE_INPUT   -1

#define UFO_FILTER_ERROR ufo_filter_error_quark()
GQuark ufo_filter_error_quark(void);

typedef enum {
    UFO_FILTER_ERROR_INSUFFICIENTINPUTS,
    UFO_FILTER_ERROR_INSUFFICIENTOUTPUTS,
    UFO_FILTER_ERROR_NUMDIMSMISMATCH,
    UFO_FILTER_ERROR_NOSUCHINPUT,
    UFO_FILTER_ERROR_NOSUCHOUTPUT,
    UFO_FILTER_ERROR_INITIALIZATION,
    UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED
} UfoFilterError;

typedef struct _UfoFilter           UfoFilter;
typedef struct _UfoFilterClass      UfoFilterClass;
typedef struct _UfoFilterPrivate    UfoFilterPrivate;
typedef struct _UfoInputParameter   UfoInputParameter;
typedef struct _UfoOutputParameter  UfoOutputParameter;

/**
 * UfoFilterConditionFunc:
 * @value: Property value that should be checked
 * @user_data: user data passed to the function
 *
 * A function that returns a boolean value depending on the input property.
 *
 * Returns: %TRUE if property satisfies condition else %FALSE
 *
 * Since: 0.2
 */
typedef gboolean (*UfoFilterConditionFunc) (GValue *value, gpointer user_data);

/**
 * UfoInputParameter:
 * @n_dims: Number of dimension the input accept
 * @n_expected_items: Number of expected items. Use UFO_FILTER_INFINITE_INPUT to
 *      accept a data stream.
 * @n_fetched_items: Number of fetched items.
 *
 * Data structure for describing the parameters of an input as used by
 * ufo_filter_register_inputs().
 */
struct _UfoInputParameter {
    /* To be initialized by filter implementation */
    guint   n_dims;
    gint    n_expected_items;

    /* Do not have to be set */
    gint    n_fetched_items;
};

/**
 * UfoOutputParameter:
 * @n_dims: Number of dimension the input accept
 *
 * Data structure for describing the parameters of an output as used by
 * ufo_filter_register_outputs().
 */
struct _UfoOutputParameter {
    guint   n_dims;
};

/**
 * UfoFilter:
 *
 * The contents of this object is opaque to the user.
 */
struct _UfoFilter {
    /*< private >*/
    GObject parent;

    UfoFilterPrivate *priv;
};

/**
 * UfoFilterClass:
 * @parent: the parent class
 * @initialize: the @initialize function is called by an UfoBaseScheduler to set
 *      up a filter before actual execution happens. It receives the first input
 *      data to which the filter can get adjust.
 * @process_cpu: this method implements data processing on CPU.
 * @process_gpu: this method implements data processing on GPU.
 *
 * The class structure for the UfoFilter type.
 */
struct _UfoFilterClass {
    GObjectClass parent;

    void (*initialize)       (UfoFilter   *filter,
                              UfoBuffer   *input[],
                              guint      **output_dim_sizes,
                              GError     **error);
    void (*process_cpu)      (UfoFilter   *filter,
                              UfoBuffer   *input[],
                              UfoBuffer   *output[],
                              gpointer     cmd_queue,
                              GError     **error);
    void (*process_gpu)      (UfoFilter   *filter,
                              UfoBuffer   *input[],
                              UfoBuffer   *output[],
                              gpointer     cmd_queue,
                              GError     **error);
};

void                ufo_filter_initialize               (UfoFilter              *filter,
                                                         UfoBuffer              *input[],
                                                         guint                 **output_dim_sizes,
                                                         GError                **error);
void                ufo_filter_set_resource_manager     (UfoFilter              *filter,
                                                         UfoResourceManager     *manager);
UfoResourceManager *ufo_filter_get_resource_manager     (UfoFilter              *filter);
void                ufo_filter_set_profiler             (UfoFilter              *filter,
                                                         UfoProfiler            *profiler);
UfoProfiler *       ufo_filter_get_profiler             (UfoFilter              *filter);
void                ufo_filter_process_cpu              (UfoFilter              *filter,
                                                         UfoBuffer              *input[],
                                                         UfoBuffer              *output[],
                                                         gpointer                cmd_queue,
                                                         GError                **error);
void                ufo_filter_process_gpu              (UfoFilter              *filter,
                                                         UfoBuffer              *input[],
                                                         UfoBuffer              *output[],
                                                         gpointer                cmd_queue,
                                                         GError                **error);
void                ufo_filter_set_plugin_name          (UfoFilter              *filter,
                                                         const gchar            *plugin_name);
const gchar*        ufo_filter_get_plugin_name          (UfoFilter              *filter);
void                ufo_filter_register_inputs          (UfoFilter              *filter,
                                                         guint                   n_inputs,
                                                         UfoInputParameter      *input_parameters);
void                ufo_filter_register_outputs         (UfoFilter              *filter,
                                                         guint                   n_outputs,
                                                         UfoOutputParameter     *output_parameters);
UfoInputParameter  *ufo_filter_get_input_parameters     (UfoFilter              *filter);
UfoOutputParameter *ufo_filter_get_output_parameters    (UfoFilter              *filter);
guint               ufo_filter_get_num_inputs           (UfoFilter              *filter);
guint               ufo_filter_get_num_outputs          (UfoFilter              *filter);
void                ufo_filter_set_output_channel       (UfoFilter              *filter,
                                                         guint                   port,
                                                         UfoChannel             *channel);
UfoChannel *        ufo_filter_get_output_channel       (UfoFilter              *filter,
                                                         guint                   port);
void                ufo_filter_set_input_channel        (UfoFilter              *filter,
                                                         guint                   port,
                                                         UfoChannel             *channel);
UfoChannel *        ufo_filter_get_input_channel        (UfoFilter              *filter,
                                                         guint                   port);
void                ufo_filter_wait_until               (UfoFilter              *filter,
                                                         GParamSpec             *pspec,
                                                         UfoFilterConditionFunc  condition,
                                                         gpointer                user_data);
GType               ufo_filter_get_type                 (void);

G_END_DECLS

#endif
