=====================
UFO 0.3 API reference
=====================

UfoArchGraph
============

.. c:type:: UfoArchGraph

    Graph structure that describes the relation between hardware
    nodes. The contents of the :c:type:`UfoArchGraph` structure are
    private and should only be accessed via the provided API.


.. c:function:: UfoGraph* ufo_arch_graph_new(UfoResources* resources, GList* remote_addresses)


    :param resources: An initialized :c:type:`UfoResources` object
    :param remote_addresses: A :c:type:`GList` containing address strings.

    :returns: A new :c:type:`UfoArchGraph`.


.. c:function:: guint ufo_arch_graph_get_num_cpus(UfoArchGraph* self)


    :returns: Number of CPU nodes in ``graph``.


.. c:function:: guint ufo_arch_graph_get_num_gpus(UfoArchGraph* self)


    :returns: Number of GPU nodes in ``graph``.


.. c:function:: guint ufo_arch_graph_get_num_remotes(UfoArchGraph* self)


    :returns: Number of remote nodes in ``graph``.


.. c:function:: GList* ufo_arch_graph_get_gpu_nodes(UfoArchGraph* self)

    #UfoGpuNode elements in ``graph``.

    :returns: A list of


.. c:function:: GList* ufo_arch_graph_get_remote_nodes(UfoArchGraph* self)

    #UfoRemoteNode elements in ``graph``.

    :returns: A list of


UfoBuffer
=========

.. c:type:: UfoBuffer

    Represents n-dimensional data. The contents of the
    :c:type:`UfoBuffer` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoBuffer* ufo_buffer_new(UfoRequisition* requisition, gpointer context)

    Create a new :c:type:`UfoBuffer`.

    :param requisition: size requisition
    :param context: cl_context to use for creating the device array

    :returns: A new :c:type:`UfoBuffer` with the given dimensions.


.. c:function:: void ufo_buffer_resize(UfoBuffer* self, UfoRequisition* requisition)

    Resize an existing buffer. If the new requisition has the same
    size as before, resizing is a no-op.

    :param requisition: A :c:type:`UfoRequisition` structure


.. c:function:: gint ufo_buffer_cmp_dimensions(UfoBuffer* self, UfoRequisition* requisition)

    Compare the size of ``buffer`` with a given ``requisition``.

    :param requisition: #UfoRequisition

    :returns: value < 0, 0 or > 0 if requisition is smaller, equal or larger.


.. c:function:: void ufo_buffer_get_requisition(UfoBuffer* self, UfoRequisition* requisition)

    Return the size of ``buffer``.

    :param requisition: A location to store the requisition of ``buffer``


.. c:function:: gsize ufo_buffer_get_size(UfoBuffer* self)

    Get the number of bytes of raw data that is managed by the
    ``buffer``.

    :returns: The size of ``buffer``'s data.


.. c:function:: void ufo_buffer_copy(UfoBuffer* self, UfoBuffer* dst)

    Copy contents of ``src`` to ``dst``. The final memory location is
    determined by the destination buffer.

    :param dst: Destination :c:type:`UfoBuffer`


.. c:function:: UfoBuffer* ufo_buffer_dup(UfoBuffer* self)

    Create a new buffer with the same requisition as ``buffer``. Note,
    that this is not a copy of ``buffer``!

    :returns: A :c:type:`UfoBuffer` with the same size as ``buffer``.


.. c:function:: gfloat* ufo_buffer_get_host_array(UfoBuffer* self, gpointer cmd_queue)

    Returns a flat C-array containing the raw float data.

    :param cmd_queue: A cl_command_queue object or ``NULL``.

    :returns: Float array.


.. c:function:: gpointer ufo_buffer_get_device_array(UfoBuffer* self, gpointer cmd_queue)

    Return the current cl_mem object of ``buffer``. If the data is not
    yet in device memory, it is transfered via ``cmd_queue`` to the
    object. If ``cmd_queue`` is ``NULL``

    :param cmd_queue: A cl_command_queue object or ``NULL``.

    :returns: A cl_mem object associated with ``buffer``.


.. c:function:: void ufo_buffer_discard_location(UfoBuffer* self, UfoMemLocation location)

    Discard ``location`` and use "other" location without copying to
    it first.

    :param location: Location to discard


.. c:function:: void ufo_buffer_convert(UfoBuffer* self, UfoBufferDepth depth)

    Convert host data according to its ``depth`` to the internal
    32-bit floating point representation.

    :param depth: Source bit depth of host data


UfoBufferParam
==============

.. c:type:: UfoBufferParam



UfoConfig
=========

.. c:type:: UfoConfig

    A :c:type:`UfoConfig` provides access to run-time specific
    settings.


.. c:function:: UfoConfig* ufo_config_new()

    Create a config object.

    :returns: A new config object.


.. c:function:: void ufo_config_add_paths(UfoConfig* self, GList* paths)

    Add ``paths`` to the list of search paths for plugins and OpenCL
    kernel files.

    :param paths: List of strings


.. c:function:: GList* ufo_config_get_paths(UfoConfig* self)

    Get an array of path strings. file system paths. Use
    :c:func:`g_list_free()` to free it.

    :returns: A list of strings containing


UfoCpuNode
==========

.. c:type:: UfoCpuNode

    Main object for organizing filters. The contents of the
    :c:type:`UfoCpuNode` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_cpu_node_new(gpointer mask)


    :param mask: None

    :returns: None


.. c:function:: gpointer ufo_cpu_node_get_affinity(UfoCpuNode* self)

    Get affinity mask of ``node``.

    :returns: A pointer to the cpu_set_t mask associated with


UfoDummyTask
============

.. c:type:: UfoDummyTask

    Main object for organizing filters. The contents of the
    :c:type:`UfoDummyTask` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_dummy_task_new()


    :returns: None


UfoGpuNode
==========

.. c:type:: UfoGpuNode

    Main object for organizing filters. The contents of the
    :c:type:`UfoGpuNode` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_gpu_node_new(gpointer cmd_queue)


    :param cmd_queue: None

    :returns: None


.. c:function:: gpointer ufo_gpu_node_get_cmd_queue(UfoGpuNode* self)

    Get command queue associated with ``node``.

    :returns: A cl_command_queue object for ``node``.


UfoGraph
========

.. c:type:: UfoGraph

    Main object for organizing filters. The contents of the
    :c:type:`UfoGraph` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoGraph* ufo_graph_new()

    Create a new :c:type:`UfoGraph` object.

    :returns: A :c:type:`UfoGraph`.


.. c:function:: void ufo_graph_register_node_type(UfoGraph* self, GType type)

    Registers ``type`` to be a valid node type of this graph. If a
    type has not be an added to ``graph``, any attempt to add such a
    node will fail.

    :param type: A :c:type:`GType`


.. c:function:: GList* ufo_graph_get_registered_node_types(UfoGraph* self)

    Get all types of nodes that can be added to ``graph``. identifiers
    that can be added to ``graph``.

    :returns: A list of :c:type:`GType`


.. c:function:: void ufo_graph_connect_nodes(UfoGraph* self, UfoNode* source, UfoNode* target, gpointer label)

    Connect ``source`` with ``target`` in ``graph`` and annotate the
    edge with

    :param source: A source node
    :param target: A target node
    :param label: An arbitrary label


.. c:function:: gboolean ufo_graph_is_connected(UfoGraph* self, UfoNode* from, UfoNode* to)

    Check whether ``from`` is connected to ``to``.

    :param from: A source node
    :param to: A target node

    :returns: %TRUE if ``from`` is connected to ``to``, otherwise %FALSE.


.. c:function:: void ufo_graph_remove_edge(UfoGraph* self, UfoNode* source, UfoNode* target)

    Remove edge between ``source`` and ``target``.

    :param source: A source node
    :param target: A target node


.. c:function:: gpointer ufo_graph_get_edge_label(UfoGraph* self, UfoNode* source, UfoNode* target)

    Retrieve edge label between ``source`` and ``target``.

    :param source: Source node
    :param target: Target node

    :returns: Edge label pointer.


.. c:function:: guint ufo_graph_get_num_nodes(UfoGraph* self)

    Get number of nodes in ``graph``. The number is always divisible
    by two, because nodes are only part of a graph if member of an
    edge.

    :returns: Number of nodes.


.. c:function:: GList* ufo_graph_get_nodes(UfoGraph* self)

    added to ``graph``.

    :returns: A list of all nodes


.. c:function:: GList* ufo_graph_get_nodes_filtered(UfoGraph* self, UfoFilterPredicate func, gpointer user_data)

    Get nodes filtered by the predicate ``func``. that are marked as
    true by the predicate function ``func``.

    :param func: Predicate function to filter out nodes
    :param user_data: Data to be passed to ``func`` on invocation

    :returns: A list of all nodes


.. c:function:: guint ufo_graph_get_num_edges(UfoGraph* self)

    Get number of edges present in ``graph``.

    :returns: Number of edges.


.. c:function:: GList* ufo_graph_get_edges(UfoGraph* self)

    Get all edges contained in ``graph``. error. Release the list with
    :c:func:`g_list_free()`.

    :returns: a list of :c:type:`UfoEdge` elements or ``NULL`` on


.. c:function:: GList* ufo_graph_get_roots(UfoGraph* self)

    Get all roots of ``graph``. that do not have a predessor node.

    :returns: A list of all nodes


.. c:function:: GList* ufo_graph_get_leaves(UfoGraph* self)

    Get all leaves of ``graph``. that do not have a predessor node.

    :returns: A list of all nodes


.. c:function:: GList* ufo_graph_get_predecessors(UfoGraph* self, UfoNode* node)

    Get the all nodes connected to ``node``. nodes of ``node``. Free
    the list with :c:func:`g_list_free()` but not its elements.

    :param node: A :c:type:`UfoNode` whose predecessors are returned.

    :returns: A list with preceeding


.. c:function:: GList* ufo_graph_get_successors(UfoGraph* self, UfoNode* node)

    Get the successors of ``node``. nodes of ``node``. Free the list
    with :c:func:`g_list_free()` but not its elements.

    :param node: A :c:type:`UfoNode` whose successors are returned.

    :returns: A list with succeeding


.. c:function:: GList* ufo_graph_get_paths(UfoGraph* self, UfoFilterPredicate pred)

    Compute a list of lists that contain complete paths with nodes
    that match a predicate function. that match ``pred``.

    :param pred: A predicate function

    :returns: A list of lists with paths


.. c:function:: void ufo_graph_split(UfoGraph* self, GList* path)

    Duplicate nodes between head and tail of path and insert at the
    exact the position of where path started and ended.

    :param path: A path of nodes, preferably created with :c:func:`ufo_graph_get_paths()`.


.. c:function:: void ufo_graph_dump_dot(UfoGraph* self, gchar* filename)

    Stores a GraphViz dot representation of ``graph`` in ``filename``.

    :param filename: A string containing a filename


UfoGroup
========

.. c:type:: UfoGroup

    Main object for organizing filters. The contents of the
    :c:type:`UfoGroup` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoGroup* ufo_group_new(GList* targets, gpointer context, UfoSendPattern pattern)

    Create a new :c:type:`UfoGroup`.

    :param targets: A list of :c:type:`UfoNode` targets
    :param context: A cl_context on which the targets should operate on.
    :param pattern: Pattern to distribute data among the ``targets``

    :returns: A new :c:type:`UfoGroup`.


.. c:function:: void ufo_group_set_num_expected(UfoGroup* self, UfoTask* target, gint n_expected)


    :param target: None
    :param n_expected: None


.. c:function:: UfoBuffer* ufo_group_pop_output_buffer(UfoGroup* self, UfoRequisition* requisition)

    that must be released with
    :c:func:`ufo_group_push_output_buffer()`.

    :param requisition: Size of the buffer.

    :returns: A newly allocated buffer or a re-used buffer


.. c:function:: void ufo_group_push_output_buffer(UfoGroup* self, UfoBuffer* buffer)


    :param buffer: None


.. c:function:: UfoBuffer* ufo_group_pop_input_buffer(UfoGroup* self, UfoTask* target)

    ufo_group_push_input_buffer().

    :param target: The :c:type:`UfoTask` that is a target in ``group``

    :returns: A buffer that must be released with


.. c:function:: void ufo_group_push_input_buffer(UfoGroup* self, UfoTask* target, UfoBuffer* input)


    :param target: None
    :param input: None


.. c:function:: void ufo_group_finish(UfoGroup* self)



UfoInputTask
============

.. c:type:: UfoInputTask

    Main object for organizing filters. The contents of the
    :c:type:`UfoInputTask` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_input_task_new()


    :returns: None


.. c:function:: void ufo_input_task_stop(UfoInputTask* self)



.. c:function:: void ufo_input_task_release_input_buffer(UfoInputTask* self, UfoBuffer* buffer)


    :param buffer: None


.. c:function:: UfoBuffer* ufo_input_task_get_input_buffer(UfoInputTask* self)

    Get the input buffer to which we write the data received from the
    master remote node.

    :returns: A :c:type:`UfoBuffer` for writing input data.


UfoNodes
========

.. c:type:: UfoNode

    Main object for organizing filters. The contents of the
    :c:type:`UfoNode` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_node_new(gpointer label)


    :param label: None

    :returns: None


.. c:function:: gpointer ufo_node_get_label(UfoNode* self)

    Get arbitrary label data of ``node``.

    :returns: The label of ``node``.


.. c:function:: UfoNode* ufo_node_copy(UfoNode* self)

    Get a copy of ``node``. How "deep" the copy is, depends on the
    inherited implementation of ``node``.

    :returns: Copy of ``node``.


.. c:function:: gboolean ufo_node_equal(UfoNode* self, UfoNode* n2)


    :param n2: None

    :returns: None


UfoOutputTask
=============

.. c:type:: UfoOutputTask

    Main object for organizing filters. The contents of the
    :c:type:`UfoOutputTask` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_output_task_new(guint n_dims)


    :param n_dims: None

    :returns: None


.. c:function:: void ufo_output_task_get_output_requisition(UfoOutputTask* self, UfoRequisition* requisition)


    :param requisition: None


.. c:function:: UfoBuffer* ufo_output_task_get_output_buffer(UfoOutputTask* self)

    Get the output buffer from which we read the data to be sent to
    the master remote node.

    :returns: A :c:type:`UfoBuffer` for reading output data.


.. c:function:: void ufo_output_task_release_output_buffer(UfoOutputTask* self, UfoBuffer* buffer)


    :param buffer: None


UfoPluginManager
================

.. c:type:: UfoPluginManager

    Creates :c:type:`UfoFilter` instances by loading corresponding
    shared objects. The contents of the :c:type:`UfoPluginManager`
    structure are private and should only be accessed via the provided
    API.


.. c:function:: UfoPluginManager* ufo_plugin_manager_new(UfoConfig* config)

    Create a plugin manager object to instantiate filter objects. When
    a config object is passed to the constructor, its search-path
    property is added to the internal search paths.

    :param config: A :c:type:`UfoConfig` object or ``NULL``.

    :returns: A new plugin manager object.


.. c:function:: UfoNode* ufo_plugin_manager_get_task(UfoPluginManager* self, gchar* name)

    Load a :c:type:`UfoFilter` module and return an instance. The
    shared object name must be * constructed as "libfilter@name.so".

    :param name: Name of the plugin.

    :returns: #UfoFilter or ``NULL`` if module cannot be found


.. c:function:: GList* ufo_plugin_manager_get_all_task_names(UfoPluginManager* self)

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


UfoRemoteNode
=============

.. c:type:: UfoRemoteNode

    Main object for organizing filters. The contents of the
    :c:type:`UfoRemoteNode` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_remote_node_new(gpointer zmq_context, gchar* address)


    :param zmq_context: None
    :param address: None

    :returns: None


.. c:function:: guint ufo_remote_node_get_num_gpus(UfoRemoteNode* self)


    :returns: None


.. c:function:: void ufo_remote_node_request_setup(UfoRemoteNode* self)



.. c:function:: void ufo_remote_node_send_json(UfoRemoteNode* self, gchar* json, gsize size)


    :param json: None
    :param size: None


.. c:function:: void ufo_remote_node_get_structure(UfoRemoteNode* self, guint* n_inputs, UfoInputParam** in_params, UfoTaskMode* mode)


    :param n_inputs: None
    :param in_params: None
    :param mode: None


.. c:function:: void ufo_remote_node_send_inputs(UfoRemoteNode* self, UfoBuffer** inputs)


    :param inputs: None


.. c:function:: void ufo_remote_node_get_result(UfoRemoteNode* self, UfoBuffer* result)


    :param result: None


.. c:function:: void ufo_remote_node_get_requisition(UfoRemoteNode* self, UfoRequisition* requisition)


    :param requisition: None


.. c:function:: void ufo_remote_node_cleanup(UfoRemoteNode* self)



UfoRemoteTask
=============

.. c:type:: UfoRemoteTask

    Main object for organizing filters. The contents of the
    :c:type:`UfoRemoteTask` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoNode* ufo_remote_task_new()


    :returns: None


UfoResources
============

.. c:type:: UfoResources

    Manages OpenCL resources. The contents of the
    :c:type:`UfoResources` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoResources* ufo_resources_new(UfoConfig* config)

    Create a new :c:type:`UfoResources` instance.

    :param config: A :c:type:`UfoConfiguration` object or ``NULL``

    :returns: A new :c:type:`UfoResources`


.. c:function:: gpointer ufo_resources_get_kernel(UfoResources* self, gchar* filename, gchar* kernel)

    Loads a and builds a kernel from a file. The file is searched in
    the current working directory and all paths added through
    ufo_resources_add_paths ().

    :param filename: Name of the .cl kernel file
    :param kernel: Name of a kernel

    :returns: a cl_kernel object that is load from ``filename`` or ``NULL`` on error


.. c:function:: gpointer ufo_resources_get_kernel_from_source(UfoResources* self, gchar* source, gchar* kernel)

    Loads and builds a kernel from a string.

    :param source: OpenCL source string
    :param kernel: Name of a kernel

    :returns: a cl_kernel object that is load from ``filename``


.. c:function:: gpointer ufo_resources_get_context(UfoResources* self)

    Returns the OpenCL context object that is used by the resource
    resources. This context can be used to initialize othe third-party
    libraries.

    :returns: A cl_context object.


.. c:function:: GList* ufo_resources_get_cmd_queues(UfoResources* self)

    Get all command queues managed by ``resources``. cl_command_queue
    objects. Free with :c:func:`g_list_free()` but not its elements.

    :returns: List with


UfoScheduler
============

.. c:type:: UfoScheduler

    The base class scheduler is responsible of assigning command
    queues to filters (thus managing GPU device resources) and decide
    if to run a GPU or a CPU. The actual schedule planning can be
    overriden.


.. c:function:: UfoScheduler* ufo_scheduler_new(UfoConfig* config, GList* remotes)

    Creates a new :c:type:`UfoScheduler`.

    :param config: A :c:type:`UfoConfig` or ``NULL``
    :param remotes: A :c:type:`GList` with strings describing remote machines or ``NULL``

    :returns: A new :c:type:`UfoScheduler`


.. c:function:: void ufo_scheduler_run(UfoScheduler* self, UfoTaskGraph* task_graph)


    :param task_graph: None


.. c:function:: gpointer ufo_scheduler_get_context(UfoScheduler* self)

    Get the associated OpenCL context of ``scheduler``.

    :returns: An cl_context structure or ``NULL`` on error.


.. c:function:: void ufo_scheduler_set_task_split(UfoScheduler* self, gboolean split)

    Sets whether the task graph should be split before execution to
    increase multi GPU performance.

    :param split: %TRUE if task graph should be split


UfoTaskGraph
============

.. c:type:: UfoTaskGraph

    Main object for organizing filters. The contents of the
    :c:type:`UfoTaskGraph` structure are private and should only be
    accessed via the provided API.


.. c:function:: UfoGraph* ufo_task_graph_new()

    Create a new task graph without any nodes.

    :returns: A :c:type:`UfoGraph` that can be upcast to a :c:type:`UfoTaskGraph`.


.. c:function:: void ufo_task_graph_read_from_file(UfoTaskGraph* self, UfoPluginManager* manager, gchar* filename)

    Read a JSON configuration file to fill the structure of ``graph``.

    :param manager: A :c:type:`UfoPluginManager` used to load the filters
    :param filename: Path and filename to the JSON file


.. c:function:: void ufo_task_graph_read_from_data(UfoTaskGraph* self, UfoPluginManager* manager, gchar* json)

    Read a JSON configuration file to fill the structure of ``graph``.

    :param manager: A :c:type:`UfoPluginManager` used to load the filters
    :param json: ``NULL``-terminated string with JSON data


.. c:function:: void ufo_task_graph_save_to_json(UfoTaskGraph* self, gchar* filename)

    Save a JSON configuration file with the filter structure of
    ``graph``.

    :param filename: Path and filename to the JSON file


.. c:function:: void ufo_task_graph_map(UfoTaskGraph* self, UfoArchGraph* arch_graph)

    Map task nodes of ``task_graph`` to the processing nodes of
    ``arch_graph``. Not doing this could break execution of
    ``task_graph``.

    :param arch_graph: A :c:type:`UfoArchGraph` to which ``task_graph``'s nodes are mapped onto


.. c:function:: void ufo_task_graph_split(UfoTaskGraph* self, UfoArchGraph* arch_graph)

    Splits ``task_graph`` in a way that most of the resources in
    ``arch_graph`` can be occupied. In the simple pipeline case, the
    longest possible GPU paths are duplicated as much as there are
    GPUs in ``arch_graph``.

    :param arch_graph: A :c:type:`UfoArchGraph`


.. c:function:: void ufo_task_graph_connect_nodes(UfoTaskGraph* self, UfoTaskNode* n1, UfoTaskNode* n2)

    Connect ``n1`` with ``n2`` using ``n2``'s default input port. To
    specify any other port, use
    :c:func:`ufo_task_graph_connect_nodes_full()`.

    :param n1: A source node
    :param n2: A destination node


.. c:function:: void ufo_task_graph_connect_nodes_full(UfoTaskGraph* self, UfoTaskNode* n1, UfoTaskNode* n2, guint input)

    Connect ``n1`` with ``n2`` using ``n2``'s ``input`` port.

    :param n1: A source node
    :param n2: A destination node
    :param input: Input port of ``n2``


.. c:function:: void ufo_task_graph_fuse(UfoTaskGraph* self)

    Fuses task nodes to increase data locality.


UfoTaskNode
===========

.. c:type:: UfoTaskNode

    Main object for organizing filters. The contents of the
    :c:type:`UfoTaskNode` structure are private and should only be
    accessed via the provided API.


.. c:function:: void ufo_task_node_set_plugin_name(UfoTaskNode* self, gchar* name)


    :param name: None


.. c:function:: gchar* ufo_task_node_get_plugin_name(UfoTaskNode* self)


    :returns: None


.. c:function:: gchar* ufo_task_node_get_unique_name(UfoTaskNode* self)


    :returns: None


.. c:function:: void ufo_task_node_set_send_pattern(UfoTaskNode* self, UfoSendPattern pattern)


    :param pattern: None


.. c:function:: UfoSendPattern ufo_task_node_get_send_pattern(UfoTaskNode* self)


    :returns: None


.. c:function:: void ufo_task_node_set_num_expected(UfoTaskNode* self, guint pos, gint n_expected)


    :param pos: None
    :param n_expected: None


.. c:function:: gint ufo_task_node_get_num_expected(UfoTaskNode* self, guint pos)


    :param pos: None

    :returns: None


.. c:function:: void ufo_task_node_set_out_group(UfoTaskNode* self, UfoGroup* group)


    :param group: None


.. c:function:: UfoGroup* ufo_task_node_get_out_group(UfoTaskNode* self)

    Get the current out of ``node``. The out group is used to fetch
    the ouput buffer for ``node`` using
    :c:func:`ufo_group_pop_output_buffer()`.

    :returns: The out group of ``node``.


.. c:function:: void ufo_task_node_add_in_group(UfoTaskNode* self, guint pos, UfoGroup* group)


    :param pos: None
    :param group: None


.. c:function:: UfoGroup* ufo_task_node_get_current_in_group(UfoTaskNode* self, guint pos)

    Several nodes can be connected to input ``pos`` of ``node``.
    However, at a time currently selected input group at ``pos``.

    :param pos: Input position of ``node``

    :returns: The current in group of ``node`` for ``pos``.


.. c:function:: void ufo_task_node_switch_in_group(UfoTaskNode* self, guint pos)


    :param pos: None


.. c:function:: void ufo_task_node_set_proc_node(UfoTaskNode* self, UfoNode* proc_node)


    :param proc_node: None


.. c:function:: UfoNode* ufo_task_node_get_proc_node(UfoTaskNode* self)

    Get the associated processing node of ``node``.

    :returns: A :c:type:`UfoNode`.
