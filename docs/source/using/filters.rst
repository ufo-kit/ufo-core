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

    ./mkfilter.py --name AwesomeFoo --type TYPE

in the ``tools/`` directory, you can avoid writing that tedious GObject boiler
plate code on your own. The name is a camel-cased version of your new filter.
Depending on how your filter works you need to specify the correct type. You can
choose between:

* ``source``: Takes no input produces output (e.g. a file reader)
* ``sink``: Takes input produces no output (e.g. a file writer)
* ``process``: Takes one input at a time and produces another output.
* ``reduce``: Takes an arbitrary amount of input data and produces an arbitrary
  amount of data as soon as an internal condition is met.

You are now left with two files ``ufo-filter-awesome-foo.c`` and
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

Regardless of filter type, the ``ufo_filter_awesome_foo_init()`` method is the
*constructor* of the filter object and usually the correct place to specify the
inputs and outputs and their respective number of dimensions using
:c:func:`ufo_filter_register_inputs` and :c:func:`ufo_filter_register_outputs`.

The ``ufo_filter_awesome_foo_class_init()`` method on the other hand ist the
*class constructor* that is used to *override* virtual methods by setting
function pointers in each classes' vtable. Depending on the filter type you need
to implement different methods for producing and consuming data.

All consumption methods are passed an array of input buffers. The length is the
number of input elements as specified with :c:func:`ufo_filter_register_inputs`.
Similarly, all production methods are passed an array of output buffers having a
length specified with :c:func:`ufo_filter_register_outputs`. Both methods also
receive a ``cmd_queue`` parameter that is used to get the actual data from the
buffer.


.. highlight:: c

Implementing filters
--------------------

Implementing source filters
~~~~~~~~~~~~~~~~~~~~~~~~~~~

A source filter must implement ``generate`` and can implement ``initialize``. To
signal that no more data is produced the ``generate`` method must return
``FALSE`` otherwise ``TRUE``::

    static void
    ufo_filter_reader_initialize (UfoFilterSource   *filter,
                                  guint            **dims,
                                  GError           **error)
    {
        /* Setup necessary auxiliary data */
    }

    static gboolean
    ufo_filter_reader_generate (UfoFilterSource  *filter,
                                UfoBuffer        *output[],
                                gpointer          cmd_queue,
                                GError          **error)
    {
        gboolean more_data = TRUE;

        /* Produce data and store result in an output buffer */

        return more_data;
    }

    static void
    ufo_filter_reader_class_init (UfoFilterReaderClass *klass)
    {
        UfoFilterSourceClass *filter_class = UFO_FILTER_SOURCE_CLASS (klass);

        filter_class->initialize = ufo_filter_reader_initialize;
        filter_class->generate = ufo_filter_reader_generate_cpu;
    }

    static void
    ufo_filter_reader_init(UfoFilterReader *self)
    {
        UfoOutputParameter output_params[] = {{2}};

        /* We provide one two-dimensional output */
        ufo_filter_register_outputs (UFO_FILTER (self), 1, output_params);
    }


Implementing sink filters
~~~~~~~~~~~~~~~~~~~~~~~~~

A sink filter must implement ``consume`` and can implement ``initialize``::

    static void
    ufo_filter_writer_consume (UfoFilterSink    *filter,
                               UfoBuffer        *input[],
                               gpointer          cmd_queue,
                               GError          **error)
    {
    }

    static void
    ufo_filter_writer_class_init(UfoFilterWriterClass *klass)
    {
        UfoFilterSinkClass *filter_class = UFO_FILTER_SINK_CLASS (klass);

        filter_class->consume = ufo_filter_writer_consume;
    }

    static void
    ufo_filter_writer_init(UfoFilterWriter *self)
    {
        UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT}};

        /* We expect one two-dimensional input */
        ufo_filter_register_inputs (UFO_FILTER (self), 1, input_params);
    }


Implementing processing filters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A "normal" processing filter can implement ``initialize`` and must implement
either ``process_cpu`` or ``process_gpu`` or both::

    static void
    ufo_filter_gaussian_blur_initialize (UfoFilter  *filter,
                                         UfoBuffer  *params[],
                                         guint     **dims,
                                         GError    **error)
    {
    }

    static void
    ufo_filter_gaussian_blur_process_gpu (UfoFilter     *filter,
                                          UfoBuffer     *input[],
                                          UfoBuffer     *output[],
                                          gpointer       cmd_queue,
                                          GError       **error)
    {
    }

    static void
    ufo_filter_gaussian_blur_class_init (UfoFilterGaussianBlurClass *klass)
    {
        UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

        filter_class->initialize    = ufo_filter_gaussian_blur_initialize;
        filter_class->process_gpu   = ufo_filter_gaussian_blur_process_gpu;
    }

    static void
    ufo_filter_gaussian_blur_init (UfoFilterGaussianBlur *self)
    {
        UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT}};
        UfoOutputParameter output_params[] = {{2}};

        ufo_filter_register_inputs (UFO_FILTER (self), 1, input_params);
        ufo_filter_register_outputs (UFO_FILTER (self), 1, output_params);
    }


Implementing reduction filters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A reduction is special::

    static void
    ufo_filter_averager_initialize (UfoFilterReduce *filter,
                                    UfoBuffer       *input[],
                                    guint **dims,
                                    gfloat *default_value,
                                    GError **error)
    {
        *default_value = 0.0f;
    }

    static void
    ufo_filter_averager_collect (UfoFilterReduce *filter,
                                 UfoBuffer       *input[],
                                 UfoBuffer       *output[],
                                 gpointer         cmd_queue,
                                 GError         **error)
    {
    }

    static gboolean
    ufo_filter_averager_reduce (UfoFilterReduce *filter,
                                UfoBuffer       *output[],
                                gpointer         cmd_queue,
                                GError         **error)
    {
        return FALSE;
    }

    static void
    ufo_filter_averager_class_init(UfoFilterAveragerClass *klass)
    {
        UfoFilterReduceClass *filter_class = UFO_FILTER_REDUCE_CLASS(klass);

        filter_class->initialize = ufo_filter_averager_initialize;
        filter_class->collect = ufo_filter_averager_collect;
        filter_class->reduce = ufo_filter_averager_reduce;
    }

    static void
    ufo_filter_averager_init(UfoFilterAverager *self)
    {
        UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT}};
        UfoOutputParameter output_params[] = {{2}};

        ufo_filter_register_inputs (UFO_FILTER (self), 1, input_params);
        ufo_filter_register_outputs (UFO_FILTER (self), 1, output_params);
    }


Moreover, you can override methods from the base GObject class. The getters and
setters for properties are overridden by default but in certain cases it makes
sense to override the ``dispose`` method to unref any GObjects and the
``finalize`` method to clean-up any other resources (e.g. allocated memory,
files).



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

    pm = Ufo.PluginManager()
    reader = pm.get_filter('reader')
    writer = pm.get_filter('writer')

    # this filter applies the kernel
    cl = pm.get_filter('cl')
    cl.set_properties(file='simple.cl', kernel='invert')

    g = Ufo.Graph()
    g.connect_filters(reader, cl)
    g.connect_filters(cl, writer)

    s = Ufo.Scheduler()
    s.run(g)

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
