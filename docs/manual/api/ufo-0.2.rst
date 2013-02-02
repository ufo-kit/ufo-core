=====================
Ufo 0.2 API reference
=====================

UfoBuffer
=========

.. c:type:: UfoBuffer

    Represents n-dimensional data. The contents of the
    :c:type:`UfoBuffer` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoBuffer* ufo_buffer_new(guint num_dims, guint* dim_size)

    Create a new buffer with the given dimensions.

    :param num_dims: Number of dimensions
    :param dim_size: Size of each dimension

    :returns: A new :c:type:`UfoBuffer` with the given dimensions.


.. c:function:: void ufo_buffer_alloc_host_mem(UfoBuffer* self)

    Allocate host memory for dimensions specified in
    :c:func:`ufo_buffer_new()`.


.. c:function:: void ufo_buffer_resize(UfoBuffer* self, guint num_dims, guint* dim_size)

    Resize an existing buffer.

    :param num_dims: New number of dimensions
    :param dim_size: New dimension sizes


.. c:function:: void ufo_buffer_copy(UfoBuffer* self, UfoBuffer* dst)

    Copy the contents of %src into %dst.

    :param dst: Destination :c:type:`UfoBuffer`


.. c:function:: void ufo_buffer_transfer_id(UfoBuffer* self, UfoBuffer* to)

    Transfer id from one buffer to another.

    :param to: UfoBuffer who gets this id


.. c:function:: gsize ufo_buffer_get_size(UfoBuffer* self)

    Get size of internal data in bytes.

    :returns: Size of data


.. c:function:: gint ufo_buffer_get_id(UfoBuffer* self)

    Get internal identification.

    :returns: unique and monotone id


.. c:function:: void ufo_buffer_get_dimensions(UfoBuffer* self, guint* num_dims, guint** dim_size)

    Retrieve dimensions of buffer.

    :param num_dims: Location to store the number of dimensions.
    :param dim_size: Location to store the dimensions. If dim_size is NULL enough space is allocated to hold num_dims elements and should be freed with :c:func:`g_free()`. If dim_size is NULL, the caller must provide enough memory.


.. c:function:: void ufo_buffer_get_2d_dimensions(UfoBuffer* self, guint* width, guint* height)

    Convenience function to retrieve dimension of buffer.

    :param width: Location to store the width of the buffer
    :param height: Location to store the height of the buffer


.. c:function:: void ufo_buffer_reinterpret(UfoBuffer* self, gsize source_depth, gsize num_pixels, gboolean normalize)

    The fundamental data type of a UfoBuffer is one single-precision
    floating point per pixel. To increase performance it is possible
    to load arbitrary integer into the host array returned by
    :c:func:`ufo_buffer_get_host_array()` and convert that data with
    this method.

    :param source_depth: The number of bits that make up the original integer data type.
    :param num_pixels: Number of pixels to consider
    :param normalize: Normalize image data to range [0.0, 1.0]


.. c:function:: void ufo_buffer_fill_with_value(UfoBuffer* self, gfloat value)

    Fill buffer with the same value.

    :param value: Buffer is filled with this value


.. c:function:: void ufo_buffer_set_host_array(UfoBuffer* self, gfloat* data, gsize num_bytes)

    Fill buffer with data. This method does not take ownership of
    data, it just copies the data off of it because we never know if
    there is enough memory to hold floats of that data.

    :param data: User supplied data
    :param num_bytes: Size of data in bytes


.. c:function:: gfloat* ufo_buffer_get_host_array(UfoBuffer* self, gpointer command_queue)

    Returns a flat C-array containing the raw float data.

    :param command_queue: A cl_command_queue object.

    :returns: Float array.


.. c:function:: GTimer* ufo_buffer_get_transfer_timer(UfoBuffer* self)

    Each buffer has a timer object that measures time spent for
    transfering data between host and device.

    :returns: A :c:type:`GTimer` associated with this buffer


.. c:function:: void ufo_buffer_swap_host_arrays(UfoBuffer* self, UfoBuffer* b)

    Swap host array pointers of ``a`` and ``b`` and mark host arrays
    valid.

    :param b: A :c:type:`UfoBuffer`


.. c:function:: gpointer ufo_buffer_get_device_array(UfoBuffer* self, gpointer command_queue)

    Get OpenCL memory object that is used to up and download data.

    :param command_queue: A cl_command_queue object that is used to access the device memory.

    :returns: OpenCL memory object associated with this :c:type:`UfoBuffer`.


.. c:function:: void ufo_buffer_invalidate_gpu_data(UfoBuffer* self)

    Invalidate state of a buffer so that Data won't be synchronized
    between CPU and GPU and must be re-set again with
    ufo_buffer_set_cpu_data.


.. c:function:: void ufo_buffer_set_cl_mem(UfoBuffer* self, gpointer mem)

    Set OpenCL memory object that is used to up and download data.

    :param mem: A cl_mem object.


.. c:function:: void ufo_buffer_attach_event(UfoBuffer* self, gpointer event)

    Attach an OpenCL event to a buffer that must be finished before
    anything else can be done with this buffer.

    :param event: A cl_event object.


.. c:function:: void ufo_buffer_get_events(UfoBuffer* self, gpointer** events, guint* num_events)

    Return events currently associated with a buffer but don't release
    them from this buffer.

    :param events: Location to store pointer of events.
    :param num_events: Location to store the length of the event array.


.. c:function:: void ufo_buffer_clear_events(UfoBuffer* self)

    Clear and release events associated with a buffer


UfoBufferParam
==============

.. c:type:: UfoBufferParam



UfoChannel
==========

.. c:type:: UfoChannel

    Data transport channel between two :c:type:`UfoFilter` objects.
    The contents of the :c:type:`UfoChannel` structure are private and
    should only be accessed via the provided API.


.. c:function:: UfoChannel* ufo_channel_new()

    Creates a new :c:type:`UfoChannel`.

    :returns: A new :c:type:`UfoChannel`


.. c:function:: void ufo_channel_ref(UfoChannel* self)

    Reference a channel if to be used as an output.


.. c:function:: void ufo_channel_finish(UfoChannel* self)

    Finish using this channel and notify subsequent filters that no
    more data can be expected.


.. c:function:: void ufo_channel_finish_next(UfoChannel* self)

    Finish using this channel. If ``channel`` has a daisy-chained
    channel, it will also be finished.


.. c:function:: void ufo_channel_insert(UfoChannel* self, UfoBuffer* buffer)

    Inserts an initial ``buffer`` that can be consumed with
    :c:func:`ufo_channel_fetch_output()`.

    :param buffer: A :c:type:`UfoBuffer` to be inserted


.. c:function:: UfoBuffer* ufo_channel_fetch_input(UfoChannel* self)

    Get a new buffer from the channel that can be consumed as an
    input.  This method blocks execution as long as no new input
    buffer the available from the preceding filter. Use
    :c:func:`ufo_channel_release_input()` to return the buffer to the
    channel.

    :returns: The next :c:type:`UfoBuffer` input


.. c:function:: void ufo_channel_release_input(UfoChannel* self, UfoBuffer* buffer)

    Release a buffer that was acquired with
    :c:func:`ufo_channel_fetch_input()`.

    :param buffer: A :c:type:`UfoBuffer` acquired with :c:func:`ufo_channel_fetch_input()`


.. c:function:: UfoBuffer* ufo_channel_fetch_output(UfoChannel* self)

    Get a new buffer from the channel that can be consumed as an
    output.  This method blocks execution as long as no new input
    buffer the available from the successing filter. Use
    :c:func:`ufo_channel_release_output()` to return the buffer to the
    channel.

    :returns: The next :c:type:`UfoBuffer` for output


.. c:function:: void ufo_channel_release_output(UfoChannel* self, UfoBuffer* buffer)

    Release a buffer that was acquired with
    :c:func:`ufo_channel_fetch_output()`.

    :param buffer: A :c:type:`UfoBuffer` acquired with :c:func:`ufo_channel_fetch_output()`


.. c:function:: void ufo_channel_daisy_chain(UfoChannel* self, UfoChannel* next)

    Appends ``next`` to ``channel``, so that both become siblings and
    share the same input. You should not use this function directly as
    it makes most sense for the scheduler when connecting to filters.

    :param next: A :c:type:`UfoChannel` that is appended to ``channel``


UfoConfiguration
================

.. c:type:: UfoConfiguration

    A :c:type:`UfoConfiguration` provides access to run-time specific
    settings.


.. c:function:: UfoConfiguration* ufo_configuration_new()

    Create a configuration object.

    :returns: A new configuration object.


.. c:function:: None ufo_configuration_get_paths(UfoConfiguration* self)

    Get an array of path strings. ``NULL``-terminated array of strings
    containing file system paths. Use :c:func:`g_strfreev()` to free
    it.


UfoFilter
=========

.. c:type:: UfoFilter

    The contents of this object is opaque to the user.


.. c:function:: void ufo_filter_initialize(UfoFilter* self, UfoBuffer* input, guint** output_dim_sizes)

    This function calls the implementation for the virtual initialize
    method. The filter can use the input buffers as a hint to setup
    its own internal structures. Moreover, it needs to return size of
    each output dimension in each port as specified with
    :c:func:`ufo_filter_register_outputs()`: <programlisting> //
    register a 1-dimensional and a 2-dimensional output in
    object::init ufo_filter_register_outputs (self, 1, 2, NULL); //
    specify sizes in object::initialize output_dim_sizes[0][0] = 1024;
    output_dim_sizes[1][0] = 640; output_dim_sizes[1][1] = 480;
    </programlisting>

    :param input: An array of buffers for each input port
    :param output_dim_sizes: The size of each dimension for each output


.. c:function:: void ufo_filter_set_resource_manager(UfoFilter* self, UfoResourceManager* manager)

    Set the resource manager that this filter uses for requesting
    resources.

    :param manager: A :c:type:`UfoResourceManager`


.. c:function:: UfoResourceManager* ufo_filter_get_resource_manager(UfoFilter* self)

    Get the resource manager that this filter uses for requesting
    resources.

    :returns: A :c:type:`UfoResourceManager`


.. c:function:: void ufo_filter_set_profiler(UfoFilter* self, UfoProfiler* profiler)

    Set this filter's profiler.

    :param profiler: A :c:type:`UfoProfiler`


.. c:function:: UfoProfiler* ufo_filter_get_profiler(UfoFilter* self)

    Get this filter's profiler.

    :returns: A :c:type:`UfoProfiler`


.. c:function:: void ufo_filter_process_cpu(UfoFilter* self, UfoBuffer* input, UfoBuffer* output)

    Process input data from a buffer array on the CPU and put the
    results into buffers in the :c:type:`output` array.

    :param input: An array of buffers for each input port
    :param output: An array of buffers for each output port


.. c:function:: void ufo_filter_process_gpu(UfoFilter* self, UfoBuffer* input, UfoBuffer* output)

    Process input data from a buffer array on the GPU and put the
    results into buffers in the :c:type:`output` array. For each
    enqueue command, a %cl_event object should be created and put into
    a :c:type:`UfoEventList` that is returned at the end::

        UfoEventList *event_list = ufo_event_list_new
        (2); cl_event *events = ufo_event_list_get_event_array
        (event_list); clEnqueueNDRangeKernel(..., 0, NULL, &events[0]);
        return event_list;

    :param input: An array of buffers for each input port
    :param output: An array of buffers for each output port


.. c:function:: void ufo_filter_set_plugin_name(UfoFilter* self, gchar* plugin_name)

    Set the name of filter.

    :param plugin_name: The name of this filter.


.. c:function:: gchar* ufo_filter_get_plugin_name(UfoFilter* self)

    Get canonical name of ``filter``.

    :returns: ``NULL``-terminated string owned by the filter.


.. c:function:: gchar* ufo_filter_get_unique_name(UfoFilter* self)

    Get unique filter name consisting of the plugin name as returned
    by :c:func:`ufo_filter_get_plugin_name()`, a dash "-" and the
    address of the filter object. This can be useful to differentiate
    between several instances of the same filter.

    :returns: ``NULL``-terminated string owned by the filter.


.. c:function:: void ufo_filter_register_inputs(UfoFilter* self, guint n_inputs, UfoInputParameter* input_parameters)

    Specifies the number of dimensions and expected number of data
    elements for each input.

    :param n_inputs: Number of inputs
    :param input_parameters: An array of :c:type:`UfoInputParameter` structures


.. c:function:: void ufo_filter_register_outputs(UfoFilter* self, guint n_outputs, UfoOutputParameter* output_parameters)

    Specifies the number of dimensions for each output.

    :param n_outputs: Number of outputs
    :param output_parameters: An array of :c:type:`UfoOutputParameter` structures


.. c:function:: UfoInputParameter* ufo_filter_get_input_parameters(UfoFilter* self)

    Get input parameters. freed.

    :returns: An array of :c:type:`UfoInputParameter` structures. This array must not be


.. c:function:: UfoOutputParameter* ufo_filter_get_output_parameters(UfoFilter* self)

    Get ouput parameters. freed.

    :returns: An array of :c:type:`UfoOuputParameter` structures. This array must not be


.. c:function:: guint ufo_filter_get_num_inputs(UfoFilter* self)

    Return the number of input ports.

    :returns: Number of input ports.


.. c:function:: guint ufo_filter_get_num_outputs(UfoFilter* self)

    Return the number of output ports.

    :returns: Number of output ports.


.. c:function:: void ufo_filter_set_output_channel(UfoFilter* self, guint port, UfoChannel* channel)

    Set a filter's output channel for a certain output port.

    :param port: Output port number
    :param channel: A :c:type:`UfoChannel`.


.. c:function:: UfoChannel* ufo_filter_get_output_channel(UfoFilter* self, guint port)

    Return a filter's output channel for a certain output port.

    :param port: Output port number

    :returns: The associated output channel.


.. c:function:: void ufo_filter_set_input_channel(UfoFilter* self, guint port, UfoChannel* channel)

    Set a filter's input channel for a certain input port.

    :param port: input port number
    :param channel: A :c:type:`UfoChannel`.


.. c:function:: void ufo_filter_set_command_queue(UfoFilter* self, gpointer cmd_queue)

    Set the associated command queue.

    :param cmd_queue: A %cl_command_queue to be used for computation and data transfer


.. c:function:: gpointer ufo_filter_get_command_queue(UfoFilter* self)

    Get the associated command queue.

    :returns: A %cl_command_queue or ``NULL``


.. c:function:: UfoChannel* ufo_filter_get_input_channel(UfoFilter* self, guint port)

    Return a filter's input channel for a certain input port.

    :param port: input port number

    :returns: The associated input channel.


.. c:function:: void ufo_filter_wait_until(UfoFilter* self, GParamSpec* pspec, UfoFilterConditionFunc condition, gpointer user_data)

    Wait until a property specified by ``pspec`` fulfills
    ``condition``.

    :param pspec: The specification of the property
    :param condition: A condition function to wait until it is satisfied
    :param user_data: User data passed to the condition func


UfoFilterReduce
===============

.. c:type:: UfoFilterReduce

    The contents of this object is opaque to the user.


.. c:function:: void ufo_filter_reduce_initialize(UfoFilterReduce* self, UfoBuffer* input, guint** output_dims, gfloat* default_value)

    This function calls the implementation for the virtual initialize
    method. The filter can use the input buffers as a hint to setup
    its own internal structures. Moreover, it needs to return size of
    each output dimension in each port as specified with
    :c:func:`ufo_filter_register_outputs()`: <programlisting> //
    register a 1-dimensional and a 2-dimensional output in
    object::init ufo_filter_register_outputs (self, 1, 2, NULL); //
    specify sizes in object::initialize output_dim_sizes[0][0] = 1024;
    output_dim_sizes[1][0] = 640; output_dim_sizes[1][1] = 480;
    </programlisting> It also has to set a valid default value with
    which the output buffer is initialized.

    :param input: An array of buffers for each input port
    :param output_dims: The size of each dimension for each output
    :param default_value: The value to fill the output buffer


.. c:function:: void ufo_filter_reduce_collect(UfoFilterReduce* self, UfoBuffer* input, UfoBuffer* output)

    Process input data. The output buffer array contains the same
    buffers on each method invocation and can be used to store
    accumulated values.

    :param input: An array of buffers for each input port
    :param output: An array of buffers for each output port


.. c:function:: gboolean ufo_filter_reduce_reduce(UfoFilterReduce* self, UfoBuffer* output)

    This method calls the virtual reduce method and is called itself,
    when the input data stream has finished. The reduce method can be
    used to finalize work on the output buffers.

    :param output: An array of buffers for each output port

    :returns: TRUE if data is produced or FALSE if reduction has stopped


UfoFilterRepeater
=================

.. c:type:: UfoFilterRepeater

    The contents of this object is opaque to the user.


UfoFilterSink
=============

.. c:type:: UfoFilterSink

    The contents of this object is opaque to the user.


.. c:function:: void ufo_filter_sink_initialize(UfoFilterSink* self, UfoBuffer* input)

    This function calls the implementation for the virtual initialize
    method. The filter can use the input buffers as a hint to setup
    its own internal structures.

    :param input: An array of buffers for each input port


.. c:function:: void ufo_filter_sink_consume(UfoFilterSink* self, UfoBuffer* input)

    Process input data from a buffer array.

    :param input: An array of buffers for each input port


UfoFilterSinkDirect
===================

.. c:type:: UfoFilterSinkDirect

    The contents of this object is opaque to the user.


.. c:function:: UfoBuffer* ufo_filter_sink_direct_pop(UfoFilterSinkDirect* self)

    Get the buffer from this node. After processing the data, the
    buffer needs to be released with
    :c:func:`ufo_filter_sink_direct_release()`.

    :returns: A :c:type:`UfoBuffer` to be processed directly.


.. c:function:: void ufo_filter_sink_direct_release(UfoFilterSinkDirect* self, UfoBuffer* buffer)

    Release a buffer acquired with
    :c:func:`ufo_filter_sink_direct_pop()`.

    :param buffer: A :c:type:`UfoBuffer` acquired with :c:func:`ufo_filter_sink_direct_pop()`.


UfoFilterSource
===============

.. c:type:: UfoFilterSource

    The contents of this object is opaque to the user.


.. c:function:: void ufo_filter_source_initialize(UfoFilterSource* self, guint** output_dim_sizes)

    This function calls the implementation for the virtual initialize
    method. It needs to return size of each output dimension in each
    port as specified with :c:func:`ufo_filter_register_outputs()`:
    <programlisting> // register a 1-dimensional and a 2-dimensional
    output in object::init ufo_filter_register_outputs (self, 1, 2,
    NULL); // specify sizes in object::initialize
    output_dim_sizes[0][0] = 1024; output_dim_sizes[1][0] = 640;
    output_dim_sizes[1][1] = 480; </programlisting>

    :param output_dim_sizes: The size of each dimension for each output


.. c:function:: gboolean ufo_filter_source_generate(UfoFilterSource* self, UfoBuffer* output)

    This function calls the implementation for the virtual generate
    method. It should produce one set of outputs for each time it is
    called. If no more data is produced it must return %FALSE.

    :param output: An array of buffers for each output port

    :returns: %TRUE if data is produced, otherwise %FALSE.


UfoFilterSourceDirect
=====================

.. c:type:: UfoFilterSourceDirect

    The contents of this object is opaque to the user.


.. c:function:: void ufo_filter_source_direct_push(UfoFilterSourceDirect* self, UfoBuffer* buffer)

    Pushes a :c:type:`UfoBuffer` into this node to be processed by
    subsequent, connected filters. To stop iterating, call
    :c:func:`ufo_filter_source_direct_stop()`.

    :param buffer: A :c:type:`UfoBuffer` to be pushed into this node


.. c:function:: void ufo_filter_source_direct_stop(UfoFilterSourceDirect* self)

    Stop execution. This node cannot accept anymore and subsequent
    nodes will be notified, that data generation has stopped.


UfoFilterSplitter
=================

.. c:type:: UfoFilterSplitter

    The contents of this object is opaque to the user.


UfoGraph
========

.. c:type:: UfoGraph

    Main object for organizing filters. The contents of the
    :c:type:`UfoGraph` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoGraph* ufo_graph_new()

    Create a new :c:type:`UfoGraph`.

    :returns: A :c:type:`UfoGraph`.


.. c:function:: void ufo_graph_read_from_json(UfoGraph* self, UfoPluginManager* manager, gchar* filename)

    Read a JSON configuration file to fill the filter structure of
    ``graph``.

    :param manager: A :c:type:`UfoPluginManager` used to load the filters
    :param filename: Path and filename to the JSON file


.. c:function:: void ufo_graph_save_to_json(UfoGraph* self, gchar* filename)

    Save a JSON configuration file with the filter structure of
    ``graph``.

    :param filename: Path and filename to the JSON file


.. c:function:: void ufo_graph_connect_filters(UfoGraph* self, UfoFilter* from, UfoFilter* to)

    Connect to filters using their default input and output ports.

    :param from: Source filter
    :param to: Destination filter


.. c:function:: void ufo_graph_connect_filters_full(UfoGraph* self, UfoFilter* from, guint from_port, UfoFilter* to, guint to_port, UfoTransferMode mode)

    Connect two filters with the specified input and output ports.

    :param from: Source filter
    :param from_port: Source output port
    :param to: Destination filter
    :param to_port: Destination input port
    :param mode: Transfer mode for multiple sinks


.. c:function:: GList* ufo_graph_get_filters(UfoGraph* self)

    Return a list of all filter nodes of ``graph``.
    :c:func:`g_list_free()` when done using the list.

    :returns: List of filter nodes. Use


.. c:function:: guint ufo_graph_get_num_filters(UfoGraph* self)

    Return the number of filters connected in the graph.

    :returns: Number of filters.


.. c:function:: GList* ufo_graph_get_roots(UfoGraph* self)

    Return a list of :c:type:`UfoFilterSource` nodes in ``graph`` that
    do not have any parents. :c:func:`g_list_free()` when done using
    the list.

    :returns: List of filter nodes. Use


.. c:function:: GList* ufo_graph_get_children(UfoGraph* self, UfoFilter* filter)

    Return a list of nodes in ``graph`` that ``filter`` connects to.
    when done using the list.

    :param filter: A :c:type:`UfoFilter`

    :returns: List of filter nodes. Use :c:func:`g_list_free()`


UfoPluginManager
================

.. c:type:: UfoPluginManager

    Creates :c:type:`UfoFilter` instances by loading corresponding
    shared objects. The contents of the :c:type:`UfoPluginManager`
    structure are private and should only be accessed via the provided
    API.


.. c:function:: UfoPluginManager* ufo_plugin_manager_new(UfoConfiguration* config)

    Create a plugin manager object to instantiate filter objects. When
    a config object is passed to the constructor, its search-path
    property is added to the internal search paths.

    :param config: A :c:type:`UfoConfiguration` object or ``NULL``.

    :returns: A new plugin manager object.


.. c:function:: UfoFilter* ufo_plugin_manager_get_filter(UfoPluginManager* self, gchar* name)

    Load a :c:type:`UfoFilter` module and return an instance. The
    shared object name must be * constructed as "libfilter@name.so".

    :param name: Name of the plugin.

    :returns: #UfoFilter or ``NULL`` if module cannot be found


.. c:function:: GList* ufo_plugin_manager_get_all_filter_names(UfoPluginManager* self)

    Return a list with potential filter names that match shared
    objects in all search paths.

    :returns: List of strings with filter names


UfoProfiler
===========

.. c:type:: UfoProfiler

    The :c:type:`UfoProfiler` collects and records OpenCL events and
    stores them in a convenient format on disk or prints summaries on
    screen.


.. c:function:: UfoProfiler* ufo_profiler_new(UfoProfilerLevel level)

    Create a profiler object.

    :param level: Amount of information that should be tracked by the profiler.

    :returns: A new profiler object.


.. c:function:: void ufo_profiler_call(UfoProfiler* self, gpointer command_queue, gpointer kernel, guint work_dim, gsize* global_work_size, gsize* local_work_size)

    Execute the ``kernel`` using the command queue and execution
    parameters. The event associated with the
    :c:func:`clEnqueueNDRangeKernel()` call is recorded and may be
    used for profiling purposes later on.

    :param command_queue: A %cl_command_queue
    :param kernel: A %cl_kernel
    :param work_dim: Number of working dimensions.
    :param global_work_size: Sizes of global dimensions. The array must have at least
    :param local_work_size: Sizes of local work group dimensions. The array must have at least ``work_dim`` entries.


.. c:function:: void ufo_profiler_foreach(UfoProfiler* self, UfoProfilerFunc func, gpointer user_data)

    Iterates through the recorded events and calls ``func`` for each
    entry.

    :param func: The function to be called for an entry
    :param user_data: User parameters


.. c:function:: void ufo_profiler_start(UfoProfiler* self, UfoProfilerTimer timer)

    Start ``timer``. The timer is not reset but accumulates the time
    elapsed between :c:func:`ufo_profiler_start()` and
    :c:func:`ufo_profiler_stop()` calls.

    :param timer: Which timer to start


.. c:function:: void ufo_profiler_stop(UfoProfiler* self, UfoProfilerTimer timer)

    Stop ``timer``. The timer is not reset but accumulates the time
    elapsed between :c:func:`ufo_profiler_start()` and
    :c:func:`ufo_profiler_stop()` calls.

    :param timer: Which timer to stop


.. c:function:: gdouble ufo_profiler_elapsed(UfoProfiler* self, UfoProfilerTimer timer)

    Get the elapsed time in seconds for ``timer``.

    :param timer: Which timer to start

    :returns: Elapsed time in seconds.


UfoResourceManager
==================

.. c:type:: UfoResourceManager

    Manages OpenCL resources. The contents of the
    :c:type:`UfoResourceManager` structure are private and should only
    be accessed via the provided API.


.. c:function:: UfoResourceManager* ufo_resource_manager_new(UfoConfiguration* config)

    Create a new :c:type:`UfoResourceManager` instance.

    :param config: A :c:type:`UfoConfiguration` object or ``NULL``

    :returns: A new :c:type:`UfoResourceManager`


.. c:function:: gpointer ufo_resource_manager_get_kernel(UfoResourceManager* self, gchar* filename, gchar* kernel_name)

    Loads a and builds a kernel from a file. The file is searched in
    the current working directory and all paths added through
    ufo_resource_manager_add_paths ().

    :param filename: Name of the .cl kernel file
    :param kernel_name: Name of a kernel

    :returns: a cl_kernel object that is load from ``filename`` or ``NULL`` on error


.. c:function:: gpointer ufo_resource_manager_get_kernel_from_source(UfoResourceManager* self, gchar* source, gchar* kernel_name)

    Loads and builds a kernel from a string.

    :param source: OpenCL source string
    :param kernel_name: Name of a kernel

    :returns: a cl_kernel object that is load from ``filename``


.. c:function:: gpointer ufo_resource_manager_get_context(UfoResourceManager* self)

    Returns the OpenCL context object that is used by the resource
    manager. This context can be used to initialize othe third-party
    libraries.

    :returns: A cl_context object.


.. c:function:: void ufo_resource_manager_get_command_queues(UfoResourceManager* self, gpointer* cmd_queues, guint* num_queues)

    Return the number and actual command queues.

    :param cmd_queues: Sets pointer to command_queues array
    :param num_queues: Number of queues


.. c:function:: gint ufo_resource_manager_get_queue_number(UfoResourceManager* self, gpointer cmd_queue)

    Translate a %cl_command_queue pointer into a numerical
    representation.

    :param cmd_queue: A %cl_command_queue

    :returns: The numeral position of %cmd_queue or -1 if it is not found.


.. c:function:: gpointer ufo_resource_manager_get_command_queue(UfoResourceManager* self, guint queue)

    Return a specific command queue.

    :param queue: The number of the queue which must be less than the number returned by ufo_resource_manager_get_number_of_devices ().

    :returns: The ith cl_command_queue


.. c:function:: guint ufo_resource_manager_get_number_of_devices(UfoResourceManager* self)

    Get number of acceleration devices such as GPUs.

    :returns: Number of acceleration devices.


.. c:function:: gpointer ufo_resource_manager_memdup(UfoResourceManager* self, gpointer memobj)

    Creates a new cl_mem object with the same size and content as a
    given cl_mem object.

    :param memobj: A cl_mem object

    :returns: A new cl_mem object


.. c:function:: gpointer ufo_resource_manager_memalloc(UfoResourceManager* self, gsize size)

    Allocates a new cl_mem object with the given size.

    :param size: Size of cl_mem in bytes

    :returns: A cl_mem object


.. c:function:: UfoBuffer* ufo_resource_manager_request_buffer(UfoResourceManager* self, guint num_dims, guint* dim_size, gfloat* data, gpointer cmd_queue)

    Creates a new :c:type:`UfoBuffer` and initializes it with data on
    demand. If non-floating point data have to be uploaded, use
    :c:func:`ufo_buffer_set_host_array()` and
    :c:func:`ufo_buffer_reinterpret()` on the :c:type:`UfoBuffer`.

    :param num_dims: Number of dimensions
    :param dim_size: Size of each dimension
    :param data: Data used to initialize the buffer with, or NULL
    :param cmd_queue: If data should be copied onto the device, a cl_command_queue must be provide, or ``NULL``

    :returns: A new :c:type:`UfoBuffer` with the given dimensions


UfoScheduler
============

.. c:type:: UfoScheduler

    The base class scheduler is responsible of assigning command
    queues to filters (thus managing GPU device resources) and decide
    if to run a GPU or a CPU. The actual schedule planning can be
    overriden.


.. c:function:: UfoScheduler* ufo_scheduler_new(UfoConfiguration* config, UfoResourceManager* manager)

    Creates a new :c:type:`UfoScheduler`.

    :param config: A :c:type:`UfoConfiguration` or ``NULL``
    :param manager: A :c:type:`UfoResourceManager` or ``NULL``

    :returns: A new :c:type:`UfoScheduler`


.. c:function:: void ufo_scheduler_run(UfoScheduler* self, UfoGraph* graph)

    Start executing all filters from the ``filters`` list in their own
    threads.

    :param graph: A :c:type:`UfoGraph` object whose filters are scheduled


