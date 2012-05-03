#!/usr/bin/python

"""
This file generates GObject file templates that a filter author can use, to
implement their own filters.
"""

import sys
import string
import textwrap

SKELETON_C="""#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-resource-manager.h>
#include <ufo/ufo-filter.h>
#include <ufo/ufo-buffer.h>
#include "ufo-filter-${prefix_hyphen}.h"

/**
 * SECTION:ufo-filter-${prefix_hyphen}
 * @Short_description:
 * @Title: ${prefix_lower}
 *
 * Detailed description.
 */

struct _UfoFilter${prefix_camel}Private {
    /* add your private data here */
    /* cl_kernel kernel; */
    float example;
};

G_DEFINE_TYPE(UfoFilter${prefix_camel}, ufo_filter_${prefix_underscore}, UFO_TYPE_FILTER)

#define UFO_FILTER_${prefix_upper}_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_${prefix_upper}, UfoFilter${prefix_camel}Private))

enum {
    PROP_0,
    PROP_EXAMPLE, /* remove this or add more */
    N_PROPERTIES
};

static GParamSpec *${prefix_underscore}_properties[N_PROPERTIES] = { NULL, };


static void ufo_filter_${prefix_underscore}_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
    /*
    UfoFilter${prefix_camel} *self = UFO_FILTER_${prefix_upper}(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = ufo_resource_manager_get_kernel(manager,
            "kernel-file.cl", "kernelname", &error);

    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
    */
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_${prefix_underscore}_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoChannel *input_channel = ufo_filter_get_input_channel(filter);
    UfoChannel *output_channel = ufo_filter_get_output_channel(filter);
    UfoBuffer *input = ufo_channel_get_input_buffer(input_channel);
    UfoBuffer *output = NULL;

    /* If you provide any output, you must allocate output buffers of the
       appropriate size */
    guint dimensions[2] = { 256, 256 };
    ufo_channel_allocate_output_buffers(output_channel, 2, dimensions);

    while (input != NULL) {
        /* Use the input here */
        output = ufo_channel_get_output_buffer(output_channel);

        /* If you don't read the input and don't modify the output any more,
           you have to finalize the buffers. */
        ufo_channel_finalize_input_buffer(input_channel, input);
        ufo_channel_finalize_output_buffer(output_channel, output);

        /* Get new input */
        input = ufo_channel_get_input_buffer(input_channel);
    }

    /* Tell subsequent filters, that we are finished */
    ufo_channel_finish(output_channel);
}

static void ufo_filter_${prefix_underscore}_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilter${prefix_camel} *self = UFO_FILTER_${prefix_upper}(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_EXAMPLE:
            self->priv->example = g_value_get_double(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_${prefix_underscore}_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilter${prefix_camel} *self = UFO_FILTER_${prefix_upper}(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_EXAMPLE:
            g_value_set_double(value, self->priv->example);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_${prefix_underscore}_class_init(UfoFilter${prefix_camel}Class *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_${prefix_underscore}_set_property;
    gobject_class->get_property = ufo_filter_${prefix_underscore}_get_property;
    filter_class->initialize = ufo_filter_${prefix_underscore}_initialize;
    filter_class->process = ufo_filter_${prefix_underscore}_process;

    /* You can document properties more in-depth if you think that the
     * description given as a parameter is not enough like this: */
    /**
     * UfoFilter${prefix_camel}:example
     *
     * Information about this property
     */
    ${prefix_underscore}_properties[PROP_EXAMPLE] = 
        g_param_spec_double("example",
            "This is an example property",
            "You should definately replace this with some meaningful property",
            -1.0,   /* minimum */
             1.0,   /* maximum */
             1.0,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_EXAMPLE, ${prefix_underscore}_properties[PROP_EXAMPLE]);

    g_type_class_add_private(gobject_class, sizeof(UfoFilter${prefix_camel}Private));
}

static void ufo_filter_${prefix_underscore}_init(UfoFilter${prefix_camel} *self)
{
    UfoFilter${prefix_camel}Private *priv = self->priv = UFO_FILTER_${prefix_upper}_GET_PRIVATE(self);
    priv->example = 1.0;

    /* 
     * Use this place to register your named inputs and outputs with the number
     * of dimensions that the input is accepting and the output providing.
     * Currently all filters use the same naming scheme that consists of an
     * input/output prefix and a monotonically increasing number starting from
     * 0. 
     */
    ufo_filter_register_input(UFO_FILTER(self), "input0", 2);
    ufo_filter_register_output(UFO_FILTER(self), "output0", 2);
}

G_MODULE_EXPORT UfoFilter *ufo_filter_plugin_new(void)
{
    return g_object_new(UFO_TYPE_FILTER_${prefix_upper}, NULL);
}
"""

SKELETON_H="""#ifndef __UFO_FILTER_${prefix_upper}_H
#define __UFO_FILTER_${prefix_upper}_H

#include <glib.h>
#include <glib-object.h>

#include <ufo/ufo-filter.h>

#define UFO_TYPE_FILTER_${prefix_upper}             (ufo_filter_${prefix_underscore}_get_type())
#define UFO_FILTER_${prefix_upper}(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_${prefix_upper}, UfoFilter${prefix_camel}))
#define UFO_IS_FILTER_${prefix_upper}(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_${prefix_upper}))
#define UFO_FILTER_${prefix_upper}_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_${prefix_upper}, UfoFilter${prefix_camel}Class))
#define UFO_IS_FILTER_${prefix_upper}_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_${prefix_upper}))
#define UFO_FILTER_${prefix_upper}_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_${prefix_upper}, UfoFilter${prefix_camel}Class))

typedef struct _UfoFilter${prefix_camel}           UfoFilter${prefix_camel};
typedef struct _UfoFilter${prefix_camel}Class      UfoFilter${prefix_camel}Class;
typedef struct _UfoFilter${prefix_camel}Private    UfoFilter${prefix_camel}Private;

struct _UfoFilter${prefix_camel} {
    /*< private >*/
    UfoFilter parent_instance;

    UfoFilter${prefix_camel}Private *priv;
};

/**
 * UfoFilter${prefix_camel}Class:
 *
 * #UfoFilter${prefix_camel} class
 */
struct _UfoFilter${prefix_camel}Class {
    /*< private >*/
    UfoFilterClass parent_class;
};

GType ufo_filter_${prefix_underscore}_get_type(void);
UfoFilter *ufo_filter_plugin_new(void);

#endif
"""

if __name__ == '__main__':
    filter_name = raw_input("Name of the new filter in CamelCase (e.g. MySuperFilter): ")

    if filter_name == "":
        print "Error: You should enter a better name than nothing"
        sys.exit(1)

    if filter_name[0] not in string.ascii_letters:
        print "Error: Filter name must begin with a letter"
        sys.exit(1)

    filter_camel = filter_name[0].upper() + filter_name[1:]
    filter_hyphen = filter_name[0].lower() + filter_name[1:]
    for letter in string.ascii_uppercase:
        filter_hyphen = filter_hyphen.replace(letter, "-" + letter.lower())
    filter_underscore = filter_hyphen.replace('-', '_')
    filter_upper = filter_underscore.upper()
    filter_lower = filter_camel.lower()

    filename_map = { 
            ("ufo-filter-%s.c" % filter_hyphen) : SKELETON_C, 
            ("ufo-filter-%s.h" % filter_hyphen) : SKELETON_H }
    for filename in filename_map.keys():
        template = string.Template(filename_map[filename])
        source = template.substitute(prefix_underscore=filter_underscore,
                prefix_camel=filter_camel,
                prefix_hyphen=filter_hyphen,
                prefix_upper=filter_upper,
                prefix_lower=filter_lower)

        file = open(filename, "w")
        file.writelines(source)
        file.close()
        print "Wrote %s" % filename

    message = "If you are about to write a UFO internal filter, you should copy \
the generated files into core/filters and adapt the CMakeLists.txt file. You \
should only add the filter sources to ${ufo_SRCS} if all build dependencies are \
met for your particular plugin.  Good luck!"
    
    print ""
    print textwrap.fill(message)
