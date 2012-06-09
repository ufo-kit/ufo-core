#ifndef __UFO_FILTER_H
#define __UFO_FILTER_H

#include <glib-object.h>

#include "ufo-resource-manager.h"
#include "ufo-buffer.h"
#include "ufo-channel.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER             (ufo_filter_get_type())
#define UFO_FILTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER, UfoFilter))
#define UFO_IS_FILTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER))
#define UFO_FILTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER, UfoFilterClass))
#define UFO_IS_FILTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER))
#define UFO_FILTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER, UfoFilterClass))

#define UFO_FILTER_ERROR ufo_filter_error_quark()
GQuark ufo_filter_error_quark(void);

typedef enum {
    UFO_FILTER_ERROR_INSUFFICIENTINPUTS,
    UFO_FILTER_ERROR_INSUFFICIENTOUTPUTS,
    UFO_FILTER_ERROR_NUMDIMSMISMATCH,
    UFO_FILTER_ERROR_NOSUCHINPUT,
    UFO_FILTER_ERROR_NOSUCHOUTPUT,
    UFO_FILTER_ERROR_INITIALIZATION
} UfoFilterError;

typedef struct _UfoFilter           UfoFilter;
typedef struct _UfoFilterClass      UfoFilterClass;
typedef struct _UfoFilterPrivate    UfoFilterPrivate;

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
 * UfoFilter:
 *
 * Creates #UfoFilter instances by loading corresponding shared objects. The
 * contents of the #UfoFilter structure are private and should only be
 * accessed via the provided API.
 */
struct _UfoFilter {
    /*< private >*/
    GObject parent;

    UfoFilterPrivate *priv;
};

/**
 * UfoFilterClass:
 *
 * #UfoFilter class
 */
struct _UfoFilterClass {
    /*< private >*/
    GObjectClass parent;

    GError * (*process) (UfoFilter *filter);
    GError * (*process_cpu) (UfoFilter *filter, UfoBuffer *params[], UfoBuffer *results[], gpointer cmd_queue);
    GError * (*process_gpu) (UfoFilter *filter, UfoBuffer *params[], UfoBuffer *results[], gpointer cmd_queue);
    GError * (*initialize) (UfoFilter *filter, UfoBuffer *params[], guint **output_dim_sizes);
};

void            ufo_filter_set_plugin_name      (UfoFilter                 *filter, 
                                                 const gchar               *plugin_name);
GError*         ufo_filter_process              (UfoFilter                 *filter);
void            ufo_filter_set_gpu_affinity     (UfoFilter                 *filter, 
                                                 guint                      gpu);
gfloat          ufo_filter_get_gpu_time         (UfoFilter                 *filter);
const gchar*    ufo_filter_get_plugin_name      (UfoFilter                 *filter);
void            ufo_filter_register_input       (UfoFilter                 *filter, 
                                                 const gchar               *name, 
                                                 guint                      num_dims);
void            ufo_filter_register_output      (UfoFilter                 *filter, 
                                                 const gchar               *name, 
                                                 guint                      num_dims);
guint           ufo_filter_get_num_inputs       (UfoFilter                 *filter);
guint           ufo_filter_get_num_outputs      (UfoFilter                 *filter);
GList*          ufo_filter_get_input_num_dims   (UfoFilter                 *filter);
GList*          ufo_filter_get_output_num_dims  (UfoFilter                 *filter);
void            ufo_filter_done                 (UfoFilter                 *filter);
gboolean        ufo_filter_is_done              (UfoFilter                 *filter);
void            ufo_filter_account_gpu_time     (UfoFilter                 *filter, gpointer event);
void            ufo_filter_wait_until           (UfoFilter                 *filter, 
                                                 GParamSpec                *pspec, 
                                                 UfoFilterConditionFunc     condition, 
                                                 gpointer                   user_data);
GType           ufo_filter_get_type             (void);

G_END_DECLS

#endif
