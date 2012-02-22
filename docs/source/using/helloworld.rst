.. _using-hello-world:

===========
Hello World
===========

.. highlight:: c

A simple UFO program written in C can look like this::

    /* foo.c */
    int main(void)
    {
        g_type_init();  /* you _must_ call this! */

        UfoGraph *graph = ufo_graph_new_from_json("hello-world.json");
        ufo_graph_run(graph);
        return 0;
    }

.. highlight:: bash

You can compile this with::

    $ gcc `pkg-config --cflags --libs ufo` foo.c -o foo
    
As you can see we simply construct a new UfoGraph object from a JSON encoded
:ref:`configuration file <json-configuration>` and execute that computation
pipeline.

.. highlight:: c

Rather than loading the structure from a file, you can also construct it by
hand::

    int main(void)
    {
        g_type_init();  /* you _must_ call this! */

        UfoGraph *graph = ufo_graph_new();
        UfoFilter *reader = ufo_graph_get_filter("reader");
        UfoFilter *writer = ufo_graph_get_filter("writer");

        g_object_set(G_OBJECT(reader),
                "path", "/home/user/data/*.tif",
                "count", 5,
                NULL);

        ufo_filter_connect_to(reader, writer);
        ufo_graph_run(graph);
        return 0;
    }


Language Bindings
=================

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

A `GNOME wiki page`__ lists all available runtime bindings. A small example
written in Javascript and to be used with Gjs can be found in
``core/tests/test.js`` directory.

__ http://live.gnome.org/GObjectIntrospection/Users

.. highlight:: python

The simple example from the beginning -- with Python-GObject installed -- would
look like this::

    from gi.repository import Ufo
    graph = Ufo.Graph()
    graph.read_from_json("some-graph.json")
    graph.run()

Similarly, constructing the graph by hand maps one-to-one to the Python object
and keyword system::

    from gi.repository import Ufo
    graph = Ufo.Graph()
    reader = graph.get_filter('reader')
    writer = graph.get_filter('writer')
    reader.set_properties(path='/home/user/data/*.tif', count=5)
    reader.connect_to(writer)
    graph.run()

    
Multithreading for data parallelism
===================================

Each filter is executed in its own thread thus allowing some form of implicit
task parallelism. This property is especially useful as image processing
pipelines usually consist of a mixture of I/O and CPU bound filters
(readers/writers vs. actual computation).

On the other hand, if you intend to increase throughput with data parallelism,
you can do so in two ways. The easiest is to connect several instances in the
same way like this::

    reader = graph.get_filter('reader')
    long_computation1 = graph.get_filter('long_computation')
    long_computation2 = graph.get_filter('long_computation')
    reader.connect_to(long_computation1)
    reader.connect_to(long_computation2)
    
This is totally fine if you don't expect the final order to be the same as the
one from the reader. If you have to be sure that the order is correct, you can
use the mux/demux filter pair::

    reader.connect_to(demux)
    demux.connect_by_name('output1', long_computation1, 'default')
    demux.connect_by_name('output2', long_computation2, 'default')
    long_computation1.connect_by_name('default', mux, 'input1')
    long_computation2.connect_by_name('default', mux, 'input2')
