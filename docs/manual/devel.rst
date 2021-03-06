.. _filters:

===========================
Developing new task filters
===========================

.. default-domain:: c

UFO filters are simple shared objects that expose their ``GType`` and implement
the ``UfoFilter`` class.


Writing a task in C
===================

.. highlight:: bash

Writing a new UFO filter consists of filling out a pre-defined class structure.
To avoid writing the GObject boild plate code, you can call ::

    ufo-mkfilter AwesomeFoo

to generate the header and source file templates. The name must be a camel-cased
version of your new filter.

You are now left with two files ``ufo-awesome-foo-task.c`` and
``ufo-awesome-foo-task.h``. If you intend to distribute that filter with the
main UFO filter distribution, copy these files to ``ufo-filters/src``. If you
are not depending on any third-party library you can just add the following line
to the ``CMakeLists.txt`` file::

    set(ufofilter_SRCS
        ...
        ufo-awesome-foo-task.c
        ...)

You can compile this as usual by typing ::

    make

in the CMake build directory of ``ufo-filters``.


Initializing filters
--------------------

.. highlight:: c

Regardless of filter type, the ``ufo_awesome_foo_task_init()`` method is the
*constructor* of the filter object and the best place to setup all data that
does not depend on any input data.

The ``ufo__awesome_foo_task_class_init()`` method on the other hand ist the
*class constructor* that is used to *override* virtual methods by setting
function pointers in each classes' vtable.

You *must* override the following methods: ``ufo_awesome_task_get_num_inputs``,
``ufo_awesome_task_get_num_dimensions``, ``ufo_awesome_task_get_mode``,
``ufo_awesome_task_setup``, ``ufo_awesome_task_get_requisition``,
``ufo_awesome_task_process`` and/or ``ufo_awesome_task_generate``.

``get_num_inputs``, ``get_num_dimensions`` and ``get_mode`` are called by the
run-time in order to determine how many inputs your task expects, which
dimensions are allowed on each input and what processing mode your task runs ::

    static guint
    ufo_awesome_task_get_num_inputs (UfoTask *task)
    {
        return 1;   /* We expect only one input */
    }

    static guint
    ufo_awesome_task_get_num_dimensions (UfoTask *task, guint input)
    {
        return 2;   /* We ignore "input" and always expect 2 dimensions */
    }

    static UfoTaskMode
    ufo_awesome_task_get_mode (UfoTask *task)
    {
        /* We process one item after another */
        return UFO_TASK_MODE_PROCESSOR;
    }

The mode decides which functions of a task are called. Each task can provide a
``process`` function that takes input data and optionally writes output data and
a ``generate`` function that does not take input data but writes data. Both
functions return a boolean value to signal if data was produced or not (e.g. end
of stream):

* ``UFO_TASK_MODE_PROCESSOR``: The task reads data and optionally writes data.
  For that it must implement ``process``.
* ``UFO_TASK_MODE_GENERATOR``: The task only produces data (e.g. file readers)
  and must implement ``generate``.
* ``UFO_TASK_MODE_REDUCTOR``: The tasks reads the input stream and produces
  another output stream. Reading is accomplished by implementing ``process``
  whereas production is done by ``generate``.

``setup`` can be used to initialize data that depends on run-time resources like
OpenCL contexts etc. This method is called only *once* ::

    static void
    ufo_awesome_task_setup (UfoTask *task,
                            UfoResources *resources,
                            GError **error)
    {
        cl_context context;

        context = ufo_resources_get_context (resources);

        /*
           Do something with the context like allocating buffers or create
           kernels.
         */
    }

On the other hand, ``get_requisition`` is called on each iteration right before
``process``. It is used to determine which size an output buffer must have
depending on the inputs. For this you must fill in the ``requisition`` structure
correctly. If our output buffer needs to be as big as our input buffer we would
specify ::

    static void
    ufo_awesome_task_get_requisition (UfoTask *task,
                                      UfoBuffer **inputs,
                                      UfoRequisition *requisition)
    {
        ufo_buffer_get_requisition (inputs[0], requisition);
    }

Finally, you have to override the ``process`` method ::

    static gboolean
    ufo_awesome_task_process (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoBuffer *output,
                              UfoRequisition *requisition)
    {
        UfoGpuNode *node;
        cl_command_queue cmd_queue;
        cl_mem host_in;
        cl_mem host_out;

        /* We have to know to which GPU device we are assigned to */
        node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));

        /* Now, we can get the command queue */
        cmd_queue = ufo_gpu_node_get_cmd_queue (node);

        /* ... and get hold of the data */
        host_in = ufo_buffer_get_device_array (inputs[0], cmd_queue);
        host_out = ufo_buffer_get_device_array (output, cmd_queue);

        /* Call a kernel or do other meaningful work. */
    }

Tasks can and will be copied to speed up the computation on multi-GPU systems.
Any parameters that are accessible from the outside via a property are
automatically copied by the run-time system. To copy private data that is only
visible at the file scope, you have to override the ``UFO_NODE_CLASS`` method
``copy`` and copy the data yourself. This method is *always* called before
``setup`` so you can be assured to re-create your private data on the copied
task.

.. note::

    It is strongly encouraged that you export all your parameters as properties
    and re-build any internal data structures off of these parameters.


Actions performed after inputs have stopped
-------------------------------------------

.. highlight:: c

Every filter can listen to the ``inputs_stopped`` signal which is emitted when
no more input data will come. By registering a callback function for this
signal, your filter can execute cleanup actions which need to be done prior to
filter destruction. This is typically useful when tasks are referenced in a
multithreaded application from an outside environment, like Python. It could
happen that an outside thread waits for a task to do something with an input,
but if that input never comes, the outside thread might block forever. For
instance, this might happen when ``ufo_output_task_get_output_buffer`` of the
``OutputTask`` is called. Thus, the task listens to the ``inputs_stopped``
signal and stops blocking the outside caller when it arrives. If you need to
execute such cleanup actions, call ``g_signal_connect (self, "inputs_stopped",
(GCallback) your_callback_func, NULL)`` in the ``init`` function of your task.


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

    kernel void invert(global float *input, global float *output)
    {
        /* where are we? */
        int index = get_global_id(1) * get_global_size(0) + get_global_id(0);
        output[index] = 1.0f - input[index];
    }

.. highlight:: python

We wire this small kernel into this short Python script::

    from gi.repository import Ufo

    pm = Ufo.PluginManager()
    reader = pm.get_filter('reader')
    writer = pm.get_filter('writer')

    # this filter applies the kernel
    cl = pm.get_filter('opencl')
    cl.set_properties(filename='simple.cl', kernel='invert')

    g = Ufo.TaskGraph()
    g.connect_nodes(reader, cl)
    g.connect_nodes(cl, writer)

    s = Ufo.Scheduler()
    s.run(g)

For more information on how to write OpenCL kernels, consult the official
`OpenCL reference pages`__.

__ http://www.khronos.org/registry/cl/sdk/1.1/docs/man/xhtml/


Reporting errors at run-time
----------------------------

.. highlight:: c

From within a filter (or any library for that matter) never call functions such
as ``exit()`` or ``abort()``. This prevents the calling application from
identification of the problem as well as recovery. Instead use the builtin
``GError`` infrastructure that – as a bonus – map nicely to exceptions in
Python::

    static void
    ufo_awesome_task_setup (UfoTask *task,
                            UfoResources *resources,
                            GError **error)
    {
        if (error_condition) {
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                         "Error because of condition");
            return;
        }
    }

Note that ``g_set_error`` receives printf-style format strings which means you
should be as specific as possible with the given error message.


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
using ``ufo_filter_wait_until`` ::

    /* Somewhere in ufo_filter_process() */
    ufo_filter_wait_until(self, properties[PROP_CENTER_OF_ROTATION],
            &is_larger_than_zero, NULL);

.. warning::

    ``ufo_filter_wait_until`` might block indefinitely when the
    condition function never returns ``TRUE``.

.. seealso:: :ref:`faq-synchronize-properties`
