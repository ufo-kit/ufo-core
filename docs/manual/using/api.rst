.. _using-api:

=================================
Application programming interface
=================================

This section explains the application programming interface of UFO in order to
build programs integrating the UFO processing chain rather than using one of the
higher-level programs. For the remainder of this section, we will use both the
Python and C interfaces to illustrate the main concepts. They are functionally
equivalent, just remember that the C module prefixes ``UFO_``, ``ufo_`` and
``Ufo`` translate to the Python package ``Ufo`` and some conventions such as
constructors (ending with ``_new``) and exceptions (last parameter of type
``GError **``) translate seamlessly into their Python equivalents.

In almost all cases, the steps to set up a processing chain are the same:

1. Instantiation of a plugin manager in order to load tasks
2. Configuration and parameterization of the tasks
3. Instantiation of a task graph to specify the data flow
4. Instantiate of a scheduler to run the task graph object


Preliminaries
=============

.. highlight:: bash

We assume that UFO is built and installed correctly with the header (for
the C API) and the introspection files (ending in .typelib for Python). You can
verify that you compilation and linkage is possible by calling::

    pkg-config --cflags --libs ufo

which should give reasonable output. To get started you have to include the
necessary header files or import the respective Python meta module. Note that
with recent Python introspection releases, you have to specify the version of
the module you want to import before actually importing the module.

.. tabs::

    .. code-tab:: c

        #include <ufo/ufo.h>

    .. code-tab:: py

        import gi
        gi.require_version('Ufo', '0.0')
        from gi.repository import Ufo


Instantiating tasks
===================

Tasks are loaded dynamically at run-time and require a plugin manager to locate
them in the configured search paths. The very first thing you want to do is
create a new plugin manager. In GObject C every object is reference-counted, so
if you want to add and remove ownership to an object call ``g_object_ref()`` and
``g_object_unref()`` respectively. In Python this is done automatically.

.. tabs::

    .. code-tab:: c

        UfoPluginManager *pm;

        pm = ufo_plugin_manager_new ();

        /* some time later */

        g_object_unref (pm);

    .. code-tab:: py

        pm = Ufo.PluginManager()

Once you have a plugin manager, you can load new tasks (which are actually task
nodes!) by passing the name to the ``get_task()`` method. From now on, any time
you will see a ``GError **`` pointer location means as the last method argument,
you can either pass ``NULL`` to ignore it or pass the address of ``GError *``
pointer to receive information in case something went wrong. In Python these are
automatically translated to run-time exceptions you may examine.

.. tabs::

    .. code-tab:: c

        UfoTaskNode *read_task;
        UfoTaskNode *write_task;
        GError *error = NULL;

        read_task = ufo_plugin_manager_get_task (pm, "read", &error);
        write_task = ufo_plugin_manager_get_task (pm, "read", &error);

        if (error != NULL) {
            g_printerr ("error: %s\n", error->message);
            g_error_free (error);
        }

        g_object_unref (read_task);
        g_object_unref (write_task);

    .. code-tab:: py

        pm = Ufo.PluginManager()
        read_task = pm.get_task('read')
        write_task = pm.get_task('write')

The default search path is determined at built time of libufo however you can
extend that by adding additional paths to the :envvar:`UFO_PLUGIN_PATH`
environment variable.


Configuring tasks
=================

Once you loaded all required tasks you most likely want to configure them. To
make this as flexible as possible we use the GObject property mechanism which
gives us type-safe parameters that you can monitor for changes. It is possible
to set a single property, however this is a bit of a hassle in C, or many at
once:

.. tabs::

    .. code-tab:: c

        /* Setting a single value. */
        GValue path = {0,};

        g_value_init (&path, G_TYPE_STRING);
        g_value_set_string (&path, "/home/data/*.tif");
        g_object_set_property (read_task, "path", &path);
        g_value_unset (&path);

        /* Setting multiple values. Mark end with NULL. */
        g_object_set (read_task,
            "path", "/home/data/*.tif",
            "start", 10,
            "number", 100,
            NULL);

    .. code-tab:: py

        read_task.props.path = '/home/data/*.tif'
        read_task.set_properties(path='/home/data/*.tif', start=10, number=100)

The properties of the standard UFO tasks are documented at
http://ufo-filters.readthedocs.io.


Connecting tasks
================

To specify the flow from one task to another, you must connect them in a task
graph object. Note that although you could connect them in a wrong way, for
example a writer *into* a reader, you will get an error once you try to execute
such a graph.

There is the simple ``connect_nodes()`` interface for standard cases which will
connect the output of a task to the *first* input of another task and the
complete ``connect_nodes_full()`` interface which will allow you to specify the
input port of the receiving task.

.. tabs::

    .. code-tab:: c

        UfoTaskGraph *graph;

        graph = UFO_TASK_GRAPH (ufo_task_graph_new ());

        /* simple API */
        ufo_task_graph_connect_nodes (graph, read, write);
        
        /* complete API */
        ufo_task_graph_connect_nodes_full (graph, read, write, 0);

    .. code-tab:: py

        graph = Ufo.TaskGraph()
        
        # simple API
        graph.connect_nodes(read, write)

        # complete API
        graph.connect_nodes_full(read, write, 0)


Execution
=========

The last step is execution of the data flow structure. This requires a scheduler
object on which we call the ``run`` method with the task graph:

.. tabs::

    .. code-tab:: c

        UfoBaseScheduler *scheduler;

        scheduler = ufo_scheduler_new ();
        ufo_base_scheduler_run (scheduler, graph, &error);

    .. code-tab:: py

        scheduler = Ufo.Scheduler()
        scheduler.run(graph)

You can configure the execution using scheduler properties and some of the
:ref:`using-env`.


Reference
=========

To get a complete reference, please install gtk-doc and install the generated
API reference. You can view it with the Devhelp program. Another option is to
browse the automatically generated `PyGObject API reference`_.

.. _PyGObject API reference: https://lazka.github.io/pgi-docs/#Ufo-0.0
