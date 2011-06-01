#ifndef __UFO_FILTER_STRIPE_REMOVAL_H
#define __UFO_FILTER_STRIPE_REMOVAL_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_STRIPE_REMOVAL             (ufo_filter_stripe_removal_get_type())
#define UFO_FILTER_STRIPE_REMOVAL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_STRIPE_REMOVAL, UfoFilterStripeRemoval))
#define UFO_IS_FILTER_STRIPE_REMOVAL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_STRIPE_REMOVAL))
#define UFO_FILTER_STRIPE_REMOVAL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_STRIPE_REMOVAL, UfoFilterStripeRemovalClass))
#define UFO_IS_FILTER_STRIPE_REMOVAL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_STRIPE_REMOVAL))
#define UFO_FILTER_STRIPE_REMOVAL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_STRIPE_REMOVAL, UfoFilterStripeRemovalClass))

typedef struct _UfoFilterStripeRemoval           UfoFilterStripeRemoval;
typedef struct _UfoFilterStripeRemovalClass      UfoFilterStripeRemovalClass;
typedef struct _UfoFilterStripeRemovalPrivate    UfoFilterStripeRemovalPrivate;

struct _UfoFilterStripeRemoval {
    UfoFilter parent_instance;

    /* private */
    UfoFilterStripeRemovalPrivate *priv;
};

struct _UfoFilterStripeRemovalClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_stripe_removal_get_type(void);

#endif
