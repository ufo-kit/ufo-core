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
own image processing pipeline or implement a new filter.


.. toctree::
    :maxdepth: 2

    quickstart.rst
    background.rst
    cluster.rst
    cluster-mpi.rst
    filters.rst
    
