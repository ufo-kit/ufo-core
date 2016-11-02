.. _using-hello-world:

=================
Quick start guide
=================

There are three ways to specify and execute a graph of tasks. The simplest
method requires you to construct a pipeline on the command line using the
``ufo-launch`` tool which is similar to gst-launch_ from the GStreamer package.
The second method involves writing a :ref:`JSON file <json-configuration>` that
is executed by the ``ufo-runjson`` utility, the other way uses the provided
language bindings to setup the task graph specifically.

To influence the execution from the process environment check the existing
:ref:`environment variables <using-env>`.

.. _gst-launch: http://docs.gstreamer.com/display/GstSDK/gst-launch


Launching pipelines on the command line
=======================================

The ``ufo-launch`` tool receives a list of tasks separated by exclamation marks
``!`` and executes the data flow in that order. To specify task parameters, you
can add key-value pairs seperated by an equal sign. For example, to split a
multi EDF file to single TIFFs you would do::

    $ ufo-launch read path=file.edf ! write filename=out-%05i.tif

You can concatenate an arbitrary number of tasks. For example to blur the lena
image you would something like this::

    $ ufo-launch read path=lena.tif ! blur size=20 sigma=5 ! write

Some tasks receive multiple inputs which requires the use of brackets to collect
all arguments. For example, a simple flat field correction would look like
this::

    $ ufo-launch [read path=radios, read path=darks, read path=flats]! flat-field-correct ! write filename=foo.tif


Using a JSON description
========================

Our :ref:`UFO <json-configuration>` JSON format has the advantage to be
language-agnostic and portable across different versions of the UFO framework.
Let's start with a simple example, that computes the one-dimensional
Fourier-transform on a set of input files::

    {
        "nodes" : [
            {
                "plugin": "reader",
                "name": "reader",
                "properties" : { "path": "*.tif" }
            },
            {
                "plugin": "fft",
                "name": "fft"
            }
            {
                "plugin": "writer",
                "name": "writer",
                "properties" : { "filename": "fft-%05i.tif" }
            }
        ],
        "edges" : [
            {
                "from": { "name": "reader" },
                "to": { "name": "fft" }
            },
            {
                "from": { "name": "fft" },
                "to": { "name": "writer" }
            }
        ]
    }

Save this to a file named ``fft.json`` and execute it by calling the
``ufo-runjson`` tool::

    $ ufo-runjson fft.json


C interface
===========

.. highlight:: c

A simple UFO program written in C that loads the JSON description can look like
this::

    /* ufo/ufo.h is the only header allowed to be included */
    #include <ufo/ufo.h>

    int main (void)
    {
        UfoTaskGraph *graph;
        UfoScheduler *scheduler;
        UfoPluginManager *manager;

        g_type_init ();  /* you _must_ call this! */

        graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
        manager = ufo_plugin_manager_new (NULL);

        ufo_task_graph_read_from_file (graph, manager, "hello-world.json", NULL);

        scheduler = ufo_scheduler_new (NULL, NULL);
        ufo_scheduler_run (scheduler, graph, NULL);

        /* Destroy all objects */
        g_object_unref (graph);
        g_object_unref (scheduler);
        g_object_unref (manager);
        return 0;
    }

.. highlight:: bash

You can compile this with::

    $ gcc `pkg-config --cflags --libs ufo` foo.c -o foo

As you can see we simply construct a new ``UfoGraph`` object from a JSON encoded
:ref:`configuration file <json-configuration>` and execute the computation
pipeline with a ``UfoScheduler`` object.

.. highlight:: c

Rather than loading the structure from a file, you can also construct it by
hand::

    #include <ufo/ufo.h>

    int main (void)
    {
        UfoTaskGraph *graph;
        UfoPluginManager *manager;
        UfoBaseScheduler *scheduler;
        UfoTaskNode *reader;
        UfoTaskNode *writer;

    #if !(GLIB_CHECK_VERSION (2, 36, 0))
        g_type_init ();
    #endif

        graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
        manager = ufo_plugin_manager_new ();
        scheduler = ufo_scheduler_new ();
        reader = ufo_plugin_manager_get_task (manager, "read", NULL);
        writer = ufo_plugin_manager_get_task (manager, "write", NULL);

        g_object_set (G_OBJECT (reader),
                      "path", "/home/user/data/*.tif",
                      "number", 5,
                      NULL);

        ufo_task_graph_connect_nodes (graph, reader, writer);
        ufo_base_scheduler_run (scheduler, graph, NULL);
        return 0;
    }


Python Interface
================

There are no plans to support any languages with manually written language
bindings. However, UFO is a GObject-based library from which ``gir`` (GObject
Introspection) files can be generated at build time. Any language that supports
GObject Introspection and the ``gir``/``typelib`` format is thus able to
integrate UFO. No manual intervention is need if the GObject Introspection tools
are found.

Because several languages support GObject Introspection, you have to consult the
appropriate reference manuals to find out how the GObjects are mapped to their
language equivalents. Some of the options are

- Python: PyGObject_
- Javascript: Gjs_ and Seed_
- Vala has direct support using the ``--pkg`` option

.. _PyGObject: http://live.gnome.org/PyGObject
.. _Gjs: http://live.gnome.org/Gjs
.. _Seed: http://live.gnome.org/Seed

A `GNOME wiki page`__ lists all available runtime bindings.

__ http://live.gnome.org/GObjectIntrospection/Users

.. highlight:: python

The simple example from the beginning -- with Python-GObject installed -- would
look like this::

    from gi.repository import Ufo

    manager = Ufo.PluginManager()
    graph = Ufo.TaskGraph()
    scheduler = Ufo.Scheduler()

    graph.read_from_json(manager, "some-graph.json")
    scheduler.run(graph)

Similarly, constructing the graph by hand maps one-to-one to the Python object
and keyword system::

    from gi.repository import Ufo

    graph = Ufo.Graph()
    manager = Ufo.PluginManager()
    scheduler = Ufo.Scheduler()

    reader = manager.get_task('read')
    writer = manager.get_task('write')
    reader.set_properties(path='/home/user/data/*.tif', number=5)

    graph.connect_nodes(reader, writer)
    scheduler.run(graph)
