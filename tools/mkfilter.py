#!/usr/bin/python

"""
This file generates GObject file templates that a filter author can use, to
implement their own filters.
"""

import sys
import string
import textwrap

SKELETON_C="""#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-${prefix_hyphen}.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilter${prefix_camel}Private {
    /* add your private data here */
    /* cl_kernel kernel; */
    float example;
};

GType ufo_filter_${prefix_underscore}_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilter${prefix_camel}, ufo_filter_${prefix_underscore}, UFO_TYPE_FILTER);

#define UFO_FILTER_${prefix_upper}_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_${prefix_upper}, UfoFilter${prefix_camel}Private))

enum {
    PROP_0,
    PROP_EXAMPLE, /* remove this or add more */
    N_PROPERTIES
};

static GParamSpec *${prefix_underscore}_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_${prefix_underscore}_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
    /*
    UfoFilter${prefix_camel} *self = UFO_FILTER_${prefix_upper}(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "foo-kernel-file.cl", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "foo-kernel", &error);
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
    UfoFilter${prefix_camel} *self = UFO_FILTER_${prefix_upper}(filter);
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    while (1) {
        UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);

        /* We forward a finished buffer, to let succeeding filters know about
         * the end of computation. */
        if (ufo_buffer_is_finished(input)) {
            g_async_queue_push(output_queue, input);
            break;
        }

        /* Use the input here and push any output that's created. In the case of
         * a source filter, you wouldn't pop data from the input_queue but just
         * generate data with ufo_resource_manager_request_buffer() and push
         * that into output_queue. On the other hand, a sink filter would
         * release all incoming buffers from input_queue with
         * ufo_resource_manager_release_buffer() for further re-use. */

        g_async_queue_push(output_queue, input);
    }
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
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_${prefix_underscore}_set_property;
    gobject_class->get_property = ufo_filter_${prefix_underscore}_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_${prefix_underscore}_initialize;
    filter_class->process = ufo_filter_${prefix_underscore}_process;

    /* install properties */
    ${prefix_underscore}_properties[PROP_EXAMPLE] = 
        g_param_spec_double("example",
            "This is an example property",
            "You should definately replace this with some meaningful property",
            -1.0,   /* minimum */
             1.0,   /* maximum */
             1.0,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_EXAMPLE, ${prefix_underscore}_properties[PROP_EXAMPLE]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilter${prefix_camel}Private));
}

static void ufo_filter_${prefix_underscore}_init(UfoFilter${prefix_camel} *self)
{
    UfoFilter${prefix_camel}Private *priv = self->priv = UFO_FILTER_${prefix_upper}_GET_PRIVATE(self);
    priv->example = 1.0;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_${prefix_upper}, NULL);
}
"""

SKELETON_H="""#ifndef __UFO_FILTER_${prefix_upper}_H
#define __UFO_FILTER_${prefix_upper}_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

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
    UfoFilter parent_instance;

    /* private */
    UfoFilter${prefix_camel}Private *priv;
};

struct _UfoFilter${prefix_camel}Class {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_${prefix_underscore}_get_type(void);

#endif
"""

SKELETON_PLUGIN="""[UFO Plugin]
Name=${prefix_lower}
Module=$${UFO_PLUGIN_LIBRARY}
IAge=1
Authors=John Doe
Copyright=Public Domain
Website=http://ufo.kit.edu
Description=A meaningful description for this filter"""

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
            ("ufo-filter-%s.h" % filter_hyphen) : SKELETON_H,
            ("%s.ufo-plugin.in" % filter_lower) : SKELETON_PLUGIN }
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
