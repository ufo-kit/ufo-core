.. _ufo-api:

=======================================
UFO's Application Programming Interface
=======================================

Overview
========

UFOs main purpose is to build streaming setups for fast image processing. Each
setup consists of a `Graph`_ that contains several `Filter` nodes. Each Filter
is characterized by its kernel task and the number of its input and output
`Channels`_. A Filter is added to the Graph by creating a new plugin
instance with :c:func:`ufo_graph_get_filter()`. A connection between two
Filters can be established either implicitly
(:c:func:`ufo_filter_connect_to()`) or explicitly
(:c:func:`ufo_filter_connect_by_name()`) which is mandatory for filters with
more than one in- or output.

Because, each Filter is derived from the `GObject` base class, the properties
are set with :c:func:`g_object_set()`.

Channels
========

.. doxygenclass:: UfoChannel
    :project: ufo
    :members:

Filters
=======

.. doxygenclass:: UfoFilter
    :project: ufo
    :members:


Buffers
=======

.. doxygenclass:: UfoBuffer
    :project: ufo
    :members:

Graph
=====

.. doxygenclass:: UfoGraph
    :project: ufo
    :members:


Resource Manager
================

.. doxygenclass:: UfoResourceManager
    :project: ufo
    :members:

