.. _using-ufo:

=========
Using UFO
=========

UFO is a framework for high-speed image processing at Synchrotron_ beamlines. It
facilitates every available hardware device to process tomographic data as fast
as possible with on-line reconstruction as the ultimate goal.

.. _Synchrotron: http://en.wikipedia.org/wiki/Synchrotron

It is written in C using the GLib_ and GObject_ libraries to provide an
object-oriented :ref:`API <ufo-api>`.

.. _GLib: http://developer.gnome.org/glib/
.. _GObject: http://developer.gnome.org/gobject/stable/index.html

After :ref:`installing <installation>` the framework you're ready to build your
own image processing pipeline.


Hello World
===========

The easiest UFO program looks like this::

    int main(void)
    {
        g_type_init();  /* initialize GType/GObject */

        UfoGraph *graph = ufo_graph_new_from_json("hello-world.json");
        ufo_graph_run(graph);
        return 0;
    }
    
As you can see we simply construct a new UfoGraph object from a JSON encoded
:ref:`configuration file <json-configuration>` and execute that computation
pipeline.


Writing a simple OpenCL filter
==============================

The easiest way to get started, is to write OpenCL kernels using the cl-plugin.
We create a new file ``simple.cl``, that contains a simple kernel that inverts
our normalized input (you can silently ignore the ``scratch`` parameter ...)::

    __kernel void invert(__global float *data, __local float *scratch)
    {
        /* where are we? */
        int index = get_global_id(1) * get_global_size(0) + get_global_id(0);
        float inverted_value = 1.0f - data[index];
        data[index] = inverted_value;
    }

Now we want to use our little kernel and write a suitable description file
called ``simple.json``::

    {
        "type": "sequence",
        "elements": [
            { "type": "filter", "plugin": "reader",
                "properties": {
                    "prefix": "lena" 
                }
            },
            { "type": "filter", "plugin": "cl",
                "properties": {
                    "file": "simple.cl",
                    "kernel": "invert",
                    "inplace": true
                }
            },
            { "type": "filter", "plugin": "writer",
                "properties": {
                    "prefix": "foo" 
                }
            }
        ]
    }

What does this tell us? We build a simple pipeline consisting of a reader, the
cl plugin and a writer. The reader reads any TIFF files beginning with the word
`lena` and outputs them to the cl plugin. This uses our ``simple.cl`` file to
load the ``invert`` kernel which inverts the incoming data in-place. Finally,
the writer writes a file prefixed with `foo`.

Writing a filter in C
=====================

Writing a new UFO filter is simple and by calling :: 

    ./mkfilter.py

in the ``tools/`` directory, you can avoid writing that tedious GObject boiler
plate code. You are asked for a name, so lets pretend you are going to build the
latest and greatest `AwesomeFoo` filter. After calling the tool and typing
``AwesomeFoo`` you have three files: ``ufo-filter-awesome-foo.c``,
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

You can try to compile this as usual by typing ::

    make

in your CMake build directory.

In this state, the filter doesn't do anything useful. Therefore, open
``ufo-filter-awesome-foo.c`` and take a look at
``ufo_filter_awesome_foo_initialize()``. If you are about to use a GPU enhanced
filter, you may want to uncomment the lines and change the lines to include your
OpenCL file and load the appropriate OpenCL kernels.

The actual processing is done in the ``ufo_filter_awesome_foo_process()`` method
which is called exactly once per filter. Therefore, you should not return until
you receive a buffer that is marked TRUE using ``ufo_buffer_is_finished()``.  If
your filter is a sink or an ordinary input/output filter, you would pop buffers
from your input queue using ``ufo_filter_pop_buffer()`` and forward any results
using ``ufo_filter_push_buffer()``. You are free to create as much auxiliary
buffers using ``ufo_resource_manager_request_buffer()`` as you like. Any buffers
that are received but not pushed any further (e.g. a file writer) have to be
released using ``ufo_resource_manager_release_buffer()`` for further re-use.

To work with the buffer data, you would call either
``ufo_buffer_get_cpu_data()`` or ``ufo_buffer_get_gpu_data()``. You would then
get a plain ``float`` array or a ``cl_mem`` handle. The latter can be used in
conjunction with ``cl_set_kernel_arg`` to call an OpenCL kernel with the buffer
data as an argument.


Available Filters
-----------------

Several pre-written filters can be found on a :ref:`separate page <filters>`.


Language Bindings
=================

There are no plans to support any languages with manually written language
bindings. However, UFO is a GObject-based library from which ``gir`` (GObject
Introspection) files can be generated at build time. Any language that supports
GObject Introspection and the ``gir``/``typelib`` format is thus able to
integrate UFO.


Generating Introspection Files
------------------------------

No manual intervention is need if the GObject Introspection tools are found.
With a simple ::

    make install

all necessary files are installed in the correct locations.


Using GObject Introspection
---------------------------

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

A simple Python example -- with Python-GObject installed -- would look like
this::

    from gi.repository import Ufo
    graph = Ufo.Graph()
    graph.read_from_json("some-graph.json")
    graph.run()

