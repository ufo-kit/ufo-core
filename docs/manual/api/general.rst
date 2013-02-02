================
General Overview
================

.. default-domain:: c

UFOs main purpose is to build streaming setups for fast image processing. Each
setup consists of a :type:`UfoGraph` that contains several :type:`UfoFilter`
nodes. Each Filter is characterized by its kernel task and the number of its
input and output :type:`UfoChannel` elements. A Filter is added to the Graph by
creating a new plugin instance with :func:`ufo_graph_get_filter()`. A connection
between two Filters can be established either implicitly
(:func:`ufo_filter_connect_to()`) or explicitly
(:func:`ufo_filter_connect_by_name()`) which is mandatory for filters with more
than one in- or output.

Because, each Filter is derived from the `GObject` base class, the properties
are set with :func:`g_object_set()`.

