=====================
Ufo 0.2 API reference
=====================

UfoBaseScheduler
================

.. c:type:: UfoBaseScheduler

    The base class scheduler is responsible of assigning command
    queues to filters (thus managing GPU device resources) and decide
    if to run a GPU or a CPU. The actual schedule planning can be
    overriden.


.. c:function:: UfoBaseScheduler* ufo_base_scheduler_new()

    Creates a new :c:type:`UfoBaseScheduler`.

    :returns: A new :c:type:`UfoBaseScheduler`


.. c:function:: void ufo_base_scheduler_add_filter(UfoBaseScheduler* self, UfoFilter* filter)


    :param filter: None


.. c:function:: void ufo_base_scheduler_run(UfoBaseScheduler* self)

    Start executing all filters in their own threads.


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


.. c:function:: void ufo_buffer_set_dimensions(UfoBuffer* self, guint num_dims, guint* dim_size)

    Specify the size of this nd-array.

    :param num_dims: Number of dimensions
    :param dim_size: Size of each dimension


.. c:function:: void ufo_buffer_copy(UfoBuffer* self, UfoBuffer* to, gpointer command_queue)

    Copy the content of one buffer into another.

    :param to: A :c:type:`UfoBuffer` to copy into.
    :param command_queue: A cl_command_queue.


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
    :param dim_size: Location to store the dimensions. If *dim_size is NULL enough space is allocated to hold num_dims elements and should be freed with :c:func:`g_free()`. If *dim_size is NULL, the caller must provide enough memory.


.. c:function:: void ufo_buffer_get_2d_dimensions(UfoBuffer* self, guint* width, guint* height)

    Convenience function to retrieve dimension of buffer.

    :param width: Location to store the width of the buffer
    :param height: Location to store the height of the buffer


.. c:function:: void ufo_buffer_reinterpret(UfoBuffer* self, gsize source_depth, gsize num_pixels, gboolean normalize)

    The fundamental data type of a UfoBuffer is one single-precision
    floating point per pixel. To increase performance it is possible
    to load arbitrary integer data with
    :c:func:`ufo_buffer_set_cpu_data()` and convert that data with
    this method.

    :param source_depth: The number of bits that make up the original integer data type.
    :param num_pixels: Number of pixels to consider
    :param normalize: Normalize image data to range [0.0, 1.0]


.. c:function:: void ufo_buffer_set_host_array(UfoBuffer* self, float* data, gsize num_bytes)

    Fill buffer with data. This method does not take ownership of
    data, it just copies the data off of it because we never know if
    there is enough memory to hold floats of that data.

    :param data: User supplied data
    :param num_bytes: Size of data in bytes


.. c:function:: float* ufo_buffer_get_host_array(UfoBuffer* self, gpointer command_queue)

    Returns a flat C-array containing the raw float data.

    :param command_queue: A cl_command_queue object.

    :returns: Float array.


.. c:function:: GTimer* ufo_buffer_get_transfer_timer(UfoBuffer* self)


    :returns: None


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


.. c:function:: gpointer ufo_buffer_get_cl_mem(UfoBuffer* self)

    Return associated OpenCL memory object without synchronizing with
    CPU memory.

    :returns: A cl_mem object associated with this :c:type:`UfoBuffer`.


.. c:function:: void ufo_buffer_get_transfer_time(UfoBuffer* self, gulong* upload_time, gulong* download_time)

    Get statistics on how long data was copied to and from GPU
    devices.

    :param upload_time: Location to store the upload time.
    :param download_time: Location to store the download time.


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


.. c:function:: void ufo_channel_allocate_output_buffers(UfoChannel* self, guint num_dims, guint* dim_size)

    Allocate outgoing buffers with ``num_dims`` dimensions.
    ``num_dims`` must be less than or equal to
    :c:type:`UFO_BUFFER_MAX_NDIMS`.

    :param num_dims: Number of dimensions
    :param dim_size: Size of the buffers


.. c:function:: void ufo_channel_allocate_output_buffers_like(UfoChannel* self, UfoBuffer* buffer)

    Allocate outgoing buffers with dimensions given by ``buffer``.

    :param buffer: A :c:type:`UfoBuffer` whose dimensions should be used for the output buffers


.. c:function:: UfoBuffer* ufo_channel_get_input_buffer(UfoChannel* self)

    This method blocks execution as long as no new input buffer is
    readily processed by the preceding filter.

    :returns: The next :c:type:`UfoBuffer` input


.. c:function:: UfoBuffer* ufo_channel_get_output_buffer(UfoChannel* self)

    This method blocks execution as long as no new output buffer is
    readily processed by the subsequent filter.

    :returns: The next :c:type:`UfoBuffer` for output


.. c:function:: void ufo_channel_finalize_input_buffer(UfoChannel* self, UfoBuffer* buffer)

    An input buffer is owned by a filter by calling
    :c:func:`ufo_channel_get_input_buffer()` and has to be released
    again with this method, so that a preceding filter can use it
    again as an output.

    :param buffer: The :c:type:`UfoBuffer` input acquired with :c:func:`ufo_channel_get_input_buffer()`


.. c:function:: void ufo_channel_finalize_output_buffer(UfoChannel* self, UfoBuffer* buffer)

    An output buffer is owned by a filter by calling
    :c:func:`ufo_channel_get_output_buffer()` and has to be released
    again with this method, so that a subsequent filter can use it as
    an input.

    :param buffer: The :c:type:`UfoBuffer` input acquired with :c:func:`ufo_channel_get_output_buffer()`


UfoFilter
=========

.. c:type:: UfoFilter

    Creates :c:type:`UfoFilter` instances by loading corresponding
    shared objects. The contents of the :c:type:`UfoFilter` structure
    are private and should only be accessed via the provided API.


.. c:function:: void ufo_filter_initialize(UfoFilter* self, gchar* plugin_name)

    Initializes the concrete UfoFilter by giving it a name. This is
    necessary, because we cannot instantiate the object on our own as
    this is already done by the plugin manager.

    :param plugin_name: The name of this filter.


.. c:function:: GError* ufo_filter_process(UfoFilter* self)

    Execute a filter.

    :returns: None


.. c:function:: void ufo_filter_set_command_queue(UfoFilter* self, gpointer command_queue)

    Set OpenCL command queue to use for OpenCL kernel invokations. The
    command queue is usually set by UfoGraph and should not be changed
    by client code.

    :param command_queue: A cl_command_queue to be associated with this filter.


.. c:function:: gpointer ufo_filter_get_command_queue(UfoFilter* self)

    Get OpenCL command queue associated with a filter. This function
    should only be called by a derived Filter implementation

    :returns: OpenCL command queue


.. c:function:: void ufo_filter_set_gpu_affinity(UfoFilter* self, guint gpu)

    Select the GPU that this filter should use.

    :param gpu: Number of the preferred GPU.


.. c:function:: float ufo_filter_get_gpu_time(UfoFilter* self)


    :returns: Seconds that the filter used a GPU.


.. c:function:: gchar* ufo_filter_get_plugin_name(UfoFilter* self)

    Get canonical name of ``filter``.

    :returns: NULL-terminated string owned by the filter


.. c:function:: void ufo_filter_register_input(UfoFilter* self, gchar* name, guint num_dims)

    Add a new input name. Each registered input is appended to the
    filter's argument list.

    :param name: Name of appended input
    :param num_dims: Number of dimensions this input accepts.


.. c:function:: void ufo_filter_register_output(UfoFilter* self, gchar* name, guint num_dims)

    Add a new output name. Each registered output is appended to the
    filter's output list.

    :param name: Name of appended output
    :param num_dims: Number of dimensions this output provides.


.. c:function:: void ufo_filter_connect_to(UfoFilter* self, UfoFilter* destination)

    Connect filter using the default first inputs and outputs.

    :param destination: Destination :c:type:`UfoFilter`


.. c:function:: void ufo_filter_connect_by_name(UfoFilter* self, gchar* output_name, UfoFilter* destination, gchar* input_name)

    Connect output ``output_name`` of filter ``source`` with input
    ``input_name`` of filter ``destination``.

    :param output_name: Name of the source output channel
    :param destination: Destination :c:type:`UfoFilter`
    :param input_name: Name of the destination input channel


.. c:function:: gboolean ufo_filter_connected(UfoFilter* self, UfoFilter* destination)

    Check if ``source`` and ``destination`` are connected.

    :param destination: Destination :c:type:`UfoFilter`.

    :returns: TRUE if ``source`` is connected with ``destination`` else FALSE.


.. c:function:: UfoChannel* ufo_filter_get_input_channel(UfoFilter* self)

    Get default input channel

    :returns: NULL if no such channel exists, otherwise the :c:type:`UfoChannel` object.


.. c:function:: UfoChannel* ufo_filter_get_output_channel(UfoFilter* self)

    Get default output channel of filter.

    :returns: NULL if no such channel exists, otherwise the :c:type:`UfoChannel` object.


.. c:function:: UfoChannel* ufo_filter_get_input_channel_by_name(UfoFilter* self, gchar* name)

    Get input channel called ``name`` from ``filter``.

    :param name: Name of the input channel.

    :returns: NULL if no such channel exists, otherwise the :c:type:`UfoChannel` object


.. c:function:: UfoChannel* ufo_filter_get_output_channel_by_name(UfoFilter* self, gchar* name)


    :param name: Name of the output channel. Get named output channel

    :returns: NULL if no such channel exists, otherwise the :c:type:`UfoChannel` object


.. c:function:: UfoChannel** ufo_filter_get_input_channels(UfoFilter* self, guint* num_channels)

    Get the input channels associated with the filter.

    :param num_channels: Location for the number of returned channels

    :returns: The input channels in "correct" order. Free the result with ``g_free``.


.. c:function:: UfoChannel** ufo_filter_get_output_channels(UfoFilter* self, guint* num_channels)

    Get the output channels associated with the filter.

    :param num_channels: Location for the number of returned channels

    :returns: The output channels in "correct" order. Free the result with ``g_free``.


.. c:function:: void ufo_filter_done(UfoFilter* self)

    Pure producer filters have to call this method to signal that no
    more data can be expected.


.. c:function:: gboolean ufo_filter_is_done(UfoFilter* self)

    Get information about the current execution status of a pure
    producer filter. Any other filters are driven by their inputs and
    are implicitly taken as done if no data is pushed into them.

    :returns: TRUE if no more data is pushed.


.. c:function:: void ufo_filter_account_gpu_time(UfoFilter* self, gpointer event)

    If profiling is enabled, it uses the event to account the
    execution time of this event with this filter.

    :param event: Pointer to a valid cl_event


.. c:function:: void ufo_filter_wait_until(UfoFilter* self, GParamSpec* pspec, UfoFilterConditionFunc condition, gpointer user_data)

    Wait until a property specified by ``pspec`` fulfills
    ``condition``.

    :param pspec: The specification of the property
    :param condition: A condition function to wait until it is satisfied
    :param user_data: User data passed to the condition func


UfoGraph
========

.. c:type:: UfoGraph

    Main object for organizing filters. The contents of the
    :c:type:`UfoGraph` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoGraph* ufo_graph_new(gchar* paths)

    Create a new :c:type:`UfoGraph`.

    :param paths: A string with a colon-separated list of paths that are used to search for OpenCL kernel files and header files included by OpenCL kernels.

    :returns: A :c:type:`UfoGraph`.


.. c:function:: void ufo_graph_read_from_json(UfoGraph* self, gchar* filename)

    Read a JSON configuration file to fill the filter structure of
    ``graph``.

    :param filename: Path and filename to the JSON file


.. c:function:: void ufo_graph_save_to_json(UfoGraph* self, gchar* filename)

    Save a JSON configuration file with the filter structure of
    ``graph``.

    :param filename: Path and filename to the JSON file


.. c:function:: void ufo_graph_run(UfoGraph* self)

    Start execution of all UfoElements in the UfoGraph until no more
    data is produced


.. c:function:: guint ufo_graph_get_number_of_devices(UfoGraph* self)

    Query the number of used acceleration devices such as GPUs

    :returns: Number of devices


.. c:function:: GList* ufo_graph_get_filter_names(UfoGraph* self)


    :returns: list of constants.


.. c:function:: UfoFilter* ufo_graph_get_filter(UfoGraph* self, gchar* plugin_name)

    Instantiate a new filter from a given plugin.

    :param plugin_name: name of the plugin

    :returns: a :c:type:`UfoFilter`


.. c:function:: void ufo_graph_add_filter(UfoGraph* self, UfoFilter* filter, char* name)

    In the case that a filter was not created using
    :c:func:`ufo_graph_get_filter()` but in a different place, you
    have to register the filter with this method.

    :param filter: A filter that the graph should care for
    :param name: A unique human-readable name


UfoPluginManager
================

.. c:type:: UfoPluginManager

    Creates :c:type:`UfoFilter` instances by loading corresponding
    shared objects. The contents of the :c:type:`UfoPluginManager`
    structure are private and should only be accessed via the provided
    API.


.. c:function:: UfoPluginManager* ufo_plugin_manager_new()

    Create a new plugin manager object

    :returns: None


.. c:function:: void ufo_plugin_manager_add_paths(UfoPluginManager* self, gchar* paths)

    Add paths from which to search for modules

    :param paths: Zero-terminated string containing a colon-separated list of absolute paths


.. c:function:: UfoFilter* ufo_plugin_manager_get_filter(UfoPluginManager* self, gchar* name)

    Load a :c:type:`UfoFilter` module and return an instance. The
    shared object name is constructed as "libfilter@name.so".

    :param name: Name of the plugin.

    :returns: #UfoFilter or ``NULL`` if module cannot be found


.. c:function:: GList* ufo_plugin_manager_available_filters(UfoPluginManager* self)

    Return a list with potential filter names that match shared
    objects in all search paths.

    :returns: List of strings with filter names


UfoResourceManager
==================

.. c:type:: UfoResourceManager

    Manages OpenCL resources. The contents of the
    :c:type:`UfoResourceManager` structure are private and should only
    be accessed via the provided API.


.. c:function:: void ufo_resource_manager_add_paths(UfoResourceManager* self, gchar* paths)

    Each path in ``paths`` is used when searching for kernel files
    using :c:func:`ufo_resource_manager_get_kernel()` in the order
    that they are passed in.

    :param paths: A string with a list of colon-separated paths


.. c:function:: gpointer ufo_resource_manager_get_kernel(UfoResourceManager* self, gchar* filename, gchar* kernel_name)


    :param filename: Name of the .cl kernel file
    :param kernel_name: Name of a kernel

    :returns: a cl_kernel object that is load from ``filename``


.. c:function:: gpointer ufo_resource_manager_get_kernel_from_source(UfoResourceManager* self, gchar* source, gchar* kernel_name)


    :param source: None
    :param kernel_name: None

    :returns: None


.. c:function:: gpointer ufo_resource_manager_get_context(UfoResourceManager* self)

    Returns the OpenCL context object that is used by the resource
    manager. This context can be used to initialize othe third-party
    libraries.

    :returns: A cl_context object.


.. c:function:: void ufo_resource_manager_get_command_queues(UfoResourceManager* self, gpointer* command_queues, guint* num_queues)

    Return the number and actual command queues.

    :param command_queues: Sets pointer to command_queues array
    :param num_queues: Number of queues


.. c:function:: guint ufo_resource_manager_get_number_of_devices(UfoResourceManager* self)

    resource manager.

    :returns: Number of acceleration devices such as GPUs used by the


.. c:function:: gpointer ufo_resource_manager_memdup(UfoResourceManager* self, gpointer memobj)

    Creates a new cl_mem object with the same size as a given cl_mem
    object.

    :param memobj: A cl_mem object

    :returns: A new cl_mem object


.. c:function:: gpointer ufo_resource_manager_memalloc(UfoResourceManager* self, gsize size)

    Allocates a new cl_mem object with the given size.

    :param size: Size of cl_mem in bytes

    :returns: A cl_mem object


.. c:function:: UfoBuffer* ufo_resource_manager_request_buffer(UfoResourceManager* self, guint num_dims, guint* dim_size, gfloat* data, gpointer command_queue)

    Creates a new :c:type:`UfoBuffer` and initializes it with data on
    demand. If non-floating point data have to be uploaded, use
    :c:func:`ufo_buffer_set_host_array()` and
    :c:func:`ufo_buffer_reinterpret()` on the :c:type:`UfoBuffer`.

    :param num_dims: Number of dimensions
    :param dim_size: Size of each dimension
    :param data: Data used to initialize the buffer with, or NULL
    :param command_queue: If data should be copied onto the device, a cl_command_queue must be provide, or NULL

    :returns: A new :c:type:`UfoBuffer` with the given dimensions


.. c:function:: void ufo_resource_manager_release_buffer(UfoResourceManager* self, UfoBuffer* buffer)

    Release the memory of this buffer.

    :param buffer: A :c:type:`UfoBuffer`


.. c:function:: guint ufo_resource_manager_get_new_id(UfoResourceManager* self)


    :returns: None


