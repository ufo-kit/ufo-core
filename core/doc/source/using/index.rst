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
