.. _filters:

=======
Filters
=======

.. default-domain:: c

UFO filters are simple shared objects that expose their ``GType`` and implement
the :type:`UfoFilter` class. 


Writing a filter in C
=====================

.. highlight:: bash

Writing a new UFO filter is simple and by calling :: 

    ./mkfilter.py

in the ``tools/`` directory, you can avoid writing that tedious GObject boiler
plate code on your own. You are asked for a name, so lets pretend you are going
to build the latest and greatest `AwesomeFoo` filter. After calling the tool and
typing ``AwesomeFoo`` you are left with two files: ``ufo-filter-awesome-foo.c``,
``ufo-filter-awesome-foo.h``. If you intend to distribute that filter with the
main UFO filter distribution, copy these files to ``filters/src``. If you are
not depending on any third-party library you can just add the following line to
the CMakeLists.txt file::

    --- core/filters/CMakeLists.txt old
    +++ core/filters/CMakeLists.txt new
    @@ -6,6 +6,7 @@
         ufo-filter-hist.c
         ufo-filter-raw.c
         ufo-filter-scale.c
    +    ufo-filter-awesome-foo.c
    )
                           
    set(ufofilter_KERNELS)

You can compile this as usual by typing ::

    make

in your CMake build directory.

Initializing filters
--------------------

The ``ufo_filter_awesome_foo_init()`` method is the *constructor* of the filter
object and usually the correct place to specify the inputs and outputs and their
respective number of dimensions using :c:func:`ufo_filter_register_input` and
:c:func:`ufo_filter_register_output`.

The ``ufo_filter_awesome_foo_class_init()`` method on the other hand ist the
*class constructor* that is used to *override* virtual methods by setting
function pointers in each classes' vtable. The most important base methods to
override are ``ufo_filter_awesome_foo_initialize``,
``ufo_filter_awesome_foo_process_cpu`` and
``ufo_filter_awesome_foo_process_gpu`` (see `Single input filters`_) as well as
``ufo_filter_awesome_foo_process`` (see `Multiple input filters`_). Moreover, you
can override methods from the base GObject class. The getters and setters for
properties are overridden by default but in certain cases it makes sense to
override the ``dispose`` method to unref any GObjects and the ``finalize``
method to clean-up any other resources (e.g. allocated memory, files).


Single input filters
--------------------

.. highlight:: c

In the case that each input is processed independently of the others, you have
to implement ``ufo_filter_awesome_foo_process_cpu`` or
``ufo_filter_awesome_foo_process_gpu`` and set the methods in the class
constructor ``ufo_filter_awesome_foo_class_init``. The first two parameters of
these methods are arrays of pointers to input and output buffers, the last one
the command queue to retrieve the actual content. To square the inputs and add
the neighbouring elements on the CPU, you can write something like this::

    void ufo_filter_awesome_foo_process_cpu(UfoFilter *filter,
            UfoBuffer *params[], UfoBuffer *results[], gpointer cmd_queue)
    {
        UfoFilterAwesomeFooPrivate *priv = UFO_FILTER_AWESOME_FOO_GET_PRIVATE(filter);
        guint width, height;
        ufo_buffer_get_2d_dimensions(params[0], &width, &height);

        const guint N = width * height;
        gfloat *in = ufo_buffer_get_host_array(params[0], (cl_command_queue) cmd_queue);
        gfloat *out = ufo_buffer_get_host_array(results[0], (cl_command_queue) cmd_queue);

        out[0] = out[0] * out[0] + in[1];
        out[N-1] = out[N-1] * out[N-1] + in[N-2];
        for (guint i = 1; i < num_elements - 1; i++)
            out[i] = out[i] * out[i] + in[i-1] + in[i+1]; 
    }

In the GPU case you would replace :c:func:`ufo_buffer_get_host_array` with calls
to :c:func:`ufo_buffer_get_device_array` and place your OpenCL calls
accordingly. For easy deployment you should drop your kernel files into the
source directory and add them to the CMakeLists.txt file.

.. note:: The difference between ``process_cpu`` and ``process_gpu`` is purely
    syntactical. The scheduler relies on the fact that the timings correspond to
    what they are expected to be. This means you can of course call GPU code in the
    CPU process method and vice versa but will most likely just confuse the
    scheduler. For the sake of optimal performance, you should do GPU
    computations in ``process_gpu`` and CPU computations in ``process_cpu``.

If you filter produces some output our you want to setup data structures that
depend on the input size, you can use the ``ufo_filter_awesome_foo_initialize``
method that receives the same input parameters as the process methods. Any
resources that are allocated here must be freed in the ``dispose`` or
``finalize`` methods::

    static void ufo_filter_awesome_foo_initialize(UfoFilter *filter, UfoBuffer *params[])
    {
        UfoFilterAwesomeFooPrivate *priv = UFO_FILTER_AWESOME_FOO_GET_PRIVATE(filter); 
        guint w1, h1, w2, h2;
        ufo_buffer_get_2d_dimensions(params[0], &w1, &h1);
        ufo_buffer_get_2d_dimensions(params[1], &w2, &h2);

        /* We want to stack two images internally */
        guint width = MAX(w1, w2);
        guint height = h1 + h2;
        priv->stacked = g_malloc0(width * height * sizeof(gfloat));

        /* We want to output images of the same size */
        guint dims[2] = { width, height };
        ufo_channel_allocate_output_buffers(ufo_filter_get_output_channel(filter),
                2, dims);
    }

    static void ufo_filter_awesome_foo_finalize(GObject *object)
    {
        UfoFilterAwesomeFooPrivate *priv = UFO_FILTER_AWESOME_FOO_GET_PRIVATE(object); 
        g_free(priv->stacked);

        /* Now we have to call our parents finalize method */
        G_OBJECT_CLASS(ufo_filter_awesome_foo_parent_class)->finalize(object);
    }

    void ufo_filter_awesome_foo_class_init(UfoFilterAwesomeFooClass *klass)
    {
        GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
        UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

        /* ... */
        gobject_class->finalize = ufo_filter_awesome_foo_finalize;
        filter_class->initialize = ufo_filter_awesome_foo_initialize;
        /* ... */
    }

.. note:: The input that is passed to this method is actually the very first
    data set, therefore you should not mess with the actual content of the
    buffers.


Multiple input filters
----------------------

For reduction-like filters you must override ``process`` with code in
``ufo_filter_awesome_foo_process()`` which is then called exactly once per
filter. Usually, you want to retrieve the input and output channels using
:c:func:`ufo_filter_get_input_channel` and
:c:func:`ufo_filter_get_output_channel`. From these you can gather the first
data item and allocate output buffers if necessary. In a loop you are processing
your data until :c:func:`ufo_channel_get_input_buffer` returns ``NULL``. In each
loop iteration you have to finalize the input and output buffers, so that
adjacent filter nodes can (re-)use the buffers.  When processing has finished,
all associated output channels must be closed with :func:`ufo_channel_finish()`.

A very simple example, that computes the maximum of all input data, would look
like this::

    static void ufo_filter_awesome_foo_process(UfoFilter *filter)
    {
        /* Get single default channels */
        UfoChannel *input_channel = ufo_filter_get_input_channel(filter);
        UfoChannel *output_channel = ufo_filter_get_output_channel(filter);

        UfoBuffer *input = ufo_channel_get_input_buffer(input_channel);

        if (input == NULL)
            goto done;

        guint width, height;
        ufo_buffer_get_2d_dimensions(input, &width, &height);

        ufo_channel_allocate_output_buffers_like(output_channel, input);
        UfoBuffer *output = ufo_channel_get_output_buffer(output_channel);
        gfloat *out = ufo_buffer_get_host_array(output, cmd_queue);

        while (input != NULL) {
            gfloat *in = ufo_buffer_get_host_array(input, cmd_queue);

            for (guint i = 0; i < width*height; i++) {
                if (in[i] > out[i])
                    out[i] = in[i];
            }

            ufo_channel_finalize_input_buffer(input_channel, input);
            input = ufo_channel_get_input_buffer(input_channel);
        }

        /* Send the output and close the connection */
        ufo_channel_finalize_output_buffer(output_channel, output);
        ufo_channel_finish(output_channel);
    }
    

Additional source files
-----------------------

For modularity reasons, you might want to split your filter sources into
different compilation units. In order to compile and link them against the
correct library, add the following statements to the ``src/CMakeLists.txt``
file ::

    set(awesome_foo_misc_SRCS foo.c bar.c baz.c)

in case your filter is still called ``AwesomeFoo``. Notice, that the variable
name matches the plugin name with underscores between the lower-cased letters.


Writing point-based OpenCL filters
----------------------------------

.. highlight:: c

For point-based image operations it is much faster to use the cl-plugin that
writing a full-fledged C filter. We create a new file ``simple.cl``, that
contains a simple kernel that inverts our normalized input (you can silently
ignore the ``scratch`` parameter for now):: 

    __kernel void invert(__global float *data, __local float *scratch)
    {
        /* where are we? */
        int index = get_global_id(1) * get_global_size(0) + get_global_id(0);
        float inverted_value = 1.0f - data[index];
        data[index] = inverted_value;
    }

.. highlight:: python

We wire this small kernel into this short Python script::

    from gi.repository import Ufo

    g = Ufo.Graph()
    reader = graph.get_filter('reader')
    writer = graph.get_filter('writer')

    # this filter applies the kernel
    cl = graph.get_filter('cl')     
    cl.set_properties(file='simple.cl', kernel='invert')

    reader.connect_to(cl)
    cl.connect_to(writer)

For more information on how to write OpenCL kernels, consult the official
`OpenCL reference pages`__.

__ http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/


The GObject property system
===========================

.. _filters-block:

Wait until a property satisfies a condition
-------------------------------------------

.. highlight:: c

For some filters it could be important to not only wait until input buffers
arrive but also properties change their values. For example, the back-projection
should only start as soon as it is assigned a correct center-of-rotation. To
implement this, we have to define a condition function that checks if a
``GValue`` representing the current property satisfies a certain condition ::

    static gboolean is_larger_than_zero(GValue *value, gpointer user_data)
    {
        return g_value_get_float(value) > 0.0f; 
    }

As the filter installed the properties it also knows which type it is and which
``g_value_get_*()`` function to call. Now, we wait until this conditions holds
using :c:func:`ufo_filter_wait_until` ::

    /* Somewhere in ufo_filter_process() */
    ufo_filter_wait_until(self, properties[PROP_CENTER_OF_ROTATION],
            &is_larger_than_zero, NULL);

.. warning:: 

    :c:func:`ufo_filter_wait_until` might block indefinitely when the
    condition function never returns ``TRUE``.

.. seealso:: :ref:`faq-synchronize-properties`
