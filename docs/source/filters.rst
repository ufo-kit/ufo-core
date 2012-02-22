.. _filters:

=======
Filters
=======

UFO filters are simple shared objects that expose their ``GType`` and implement
the ``UfoFilter`` class. 

Writing a simple OpenCL filter
==============================

.. highlight:: c

The easiest way to get started, is to write OpenCL kernels using the cl-plugin.
We create a new file ``simple.cl``, that contains a simple kernel that inverts
our normalized input (you can silently ignore the ``scratch`` parameter for
now):: 

    __kernel void invert(__global float *data, __local float *scratch)
    {
        /* where are we? */
        int index = get_global_id(1) * get_global_size(0) + get_global_id(0);
        float inverted_value = 1.0f - data[index];
        data[index] = inverted_value;
    }

.. highlight:: python

We wire this small kernel into a short Python script::

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
    


Writing a filter in C
=====================

.. highlight:: bash

Writing a new UFO filter is simple and by calling :: 

    ./mkfilter.py

in the ``tools/`` directory, you can avoid writing that tedious GObject boiler
plate code on your own. You are asked for a name, so lets pretend you are going
to build the latest and greatest `AwesomeFoo` filter. After calling the tool and
typing ``AwesomeFoo`` you have three files: ``ufo-filter-awesome-foo.c``,
``ufo-filter-awesome-foo.h`` and ``awesomefoo.ufo-plugin.in``. If you intend to
distribute that awesome filter with the main UFO distribution, copy these files
to ``core/filters``. If you are not depending on any third-party library you can
just add the following line to the ``CMakeLists.txt`` file ::

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

In this state, the filter doesn't do anything useful. Therefore, open
``ufo-filter-awesome-foo.c`` and take a look at
``ufo_filter_awesome_foo_initialize()``. If you are about to use a GPU enhanced
filter, you may want to uncomment the lines and change the lines to include your
OpenCL file and load the appropriate OpenCL kernels.

The actual processing is done in the ``ufo_filter_awesome_foo_process()`` method
which is called exactly once per filter. Therefore, you should not return until
you receive a NULL buffer.

A filter has to use an :ref:`UfoChannel <ufo-api>` to ask for new buffers. To
retrieve an input buffer, ``ufo_channel_get_input_buffer()`` has to be called,
similarly ``ufo_channel_get_output_buffer()`` is used to retrieve a buffer where
the results are stored. As buffers are allocated between filters (using
``ufo_channel_allocate_output_buffers()``), they have to be released after usage
by calling ``ufo_channel_finalize_input_buffer()``. When processing has
finished, all associated output channels must be closed with
``ufo_channel_finish()``.

To work with the buffer data, you would call either
``ufo_buffer_get_cpu_data()`` or ``ufo_buffer_get_gpu_data()``. You would then
get a plain ``float`` array or a ``cl_mem`` handle. The latter can be used in
conjunction with ``cl_set_kernel_arg`` to call an OpenCL kernel with the buffer
data as an argument.


Additional sources
------------------

For modularity reasons, you might want to split your filter sources into
different compilation units. In order to compile and link them against the
correct library, add the following statements to the ``src/CMakeLists.txt``
file ::

    set(awesome_foo_misc_SRCS foo.c bar.c baz.c)

in case your filter is still called ``AwesomeFoo``. Notice, that the variable
name matches the plugin name with underscores before capitalized letters.

