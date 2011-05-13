#ifndef __UFO_FILTER_SINO_GENERATOR_H
#define __UFO_FILTER_SINO_GENERATOR_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_SINO_GENERATOR             (ufo_filter_sino_generator_get_type())
#define UFO_FILTER_SINO_GENERATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SINO_GENERATOR, UfoFilterSinoGenerator))
#define UFO_IS_FILTER_SINO_GENERATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SINO_GENERATOR))
#define UFO_FILTER_SINO_GENERATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SINO_GENERATOR, UfoFilterSinoGeneratorClass))
#define UFO_IS_FILTER_SINO_GENERATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SINO_GENERATOR))
#define UFO_FILTER_SINO_GENERATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SINO_GENERATOR, UfoFilterSinoGeneratorClass))

typedef struct _UfoFilterSinoGenerator           UfoFilterSinoGenerator;
typedef struct _UfoFilterSinoGeneratorClass      UfoFilterSinoGeneratorClass;
typedef struct _UfoFilterSinoGeneratorPrivate    UfoFilterSinoGeneratorPrivate;

struct _UfoFilterSinoGenerator {
    UfoFilter parent_instance;

    /* private */
    UfoFilterSinoGeneratorPrivate *priv;
};

struct _UfoFilterSinoGeneratorClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_sino_generator_get_type(void);

#endif
