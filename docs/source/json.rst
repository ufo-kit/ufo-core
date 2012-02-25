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
``edges`` array. It may also define several ``prop-set`` s for further
reference.


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

Instead of defining recurring properties for each filter, you can also use
pre-defined `Property sets`_.

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
entry is an object that contains two arrays ``from`` and ``to``. In these
arrays, the first entry is the name of the node and the second the name of the
input or output. You can omit the second entry if you want to connect the
default inputs and outputs. 

To connect the nodes defined in the `Example nodes array`_ all you have to do is ::

    "edges" : [
        { 
            "from": ["reader"],
            "to": ["writer"]
        } 
    ]

Note, that the names specify the name of the node, not the plugin which are
identical in this example.


Property sets
=============

To avoid to list the same properties for different filters over and over again,
properties can be pre-defined with a singular top-level ``prop-sets`` object.
Each key is a name that can be referenced in filter nodes using the
``prop-refs`` array. The values are ordinary ``property`` mappings::

    {
        "prop-sets" : {
            "foo-prop": {
                "path": "/home/user/path/to/projections/*.tif", 
                "count": 300
            } 
        },
        "nodes" : [
            {
                "plugin": "reader",
                "name": "reader",
                "prop-refs": ["foo-prop"]
            }
        ]
    }


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
