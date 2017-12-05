.. _json-configuration:

.. highlight:: javascript

=========================
JSON Configuration Format
=========================

JSON_ is a self-contained, human-readable data-interchange format. It is pure
Unicode text and language independent. The main structures objects containing
key/value pairs (hash-tables, dictionaries, associative arrays ...) and ordered
lists (arrays, vectors, sequences ...) of objects or values. For a complete
description you may refer to the complete reference at `json.org
<http://json.org>`_.

The configuration of a filter setup is stored in a JSON-encoded text file with a
``.json`` suffix. The root object must at least contain a ``nodes`` and an
``edges`` array ::

    { "nodes": [], "edges": [] }


Nodes array
===========

The nodes array contains filter objects that are executed on run-time.
Information how they are connected is provided in the `Edges array`_.

Filter object
-------------

A filter consists at least of a ``plugin`` key string pointing to the filter
that is going to be used and a ``name`` string field for unique identification.
Of course, plugins have to be available as a shared object in UFO's path.

.. highlight:: python

To configure the filter, the ``properties`` field can be used. This is an object
that maps string keys specifying the actual filter property to the value.
Therefore, the Python code to set a property ::

    reader = graph.get_filter('reader')
    reader.set_properties(path='/home/user/data/*.tif', count=5)

.. highlight:: javascript

translates to ::

    { "path": "/home/user/data/*.tif", "count": 5 }


Example nodes array
-------------------
 
An example node array looks like this::

    "nodes" : [
        {
            "plugin": "reader",
            "name": "reader",
            "properties" : { "path": "/home/user/data/*.tif", "count": 5 }
        },
        {
            "plugin": "writer",
            "name": "writer"
        }
    ]


Edges array
===========

The edges array specifies how the nodes in a `Nodes array`_ are connected. Each
entry is an object that contains two objects ``from`` and ``to``. In both
objects you have to specify at least the node name with the ``name`` key.
Furthermore, if there are several inputs or outputs on a node, you have to tell
which input and output to use with the ``input`` on the ``to`` node and the
``output`` key on the ``from`` node. If you omit these, they are assumed to be
0.

To connect the nodes defined in the `Example nodes array`_ all you have to do is ::

    "edges" : [
        { 
            "from": {"name": "reader"},
            "to": {"name": "writer", "input": 2}
        } 
    ]

Note, that the names specify the name of the node, not the plugin.


Loading and Saving the Graph
============================

.. highlight:: python

The ``UfoGraph`` class exports the ``ufo_graph_read_from_json`` and
``ufo_graph_read_save_to_json`` methods which are responsible for loading and
saving the graph. In Python this would simply be::

    from gi.repository import Ufo

    g1 = Ufo.Graph()

    # set up the filters using graph.get_filter() and filter.connect_to()

    g1.run()
    g1.save_to_json('graph.json')

    g2 = Ufo.Graph()
    g2.load_from_json('graph.json')
    g2.run()


.. _JSON: http://json.org
