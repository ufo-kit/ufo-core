.. _json-configuration:

=========================
JSON Configuration Format
=========================

JSON_ is a self-contained, human-readable data-interchange format. It is pure
Unicode text and language independent. The main structures are key/value pairs
(hash-tables, dictionaries, associative arrays ...) and ordered lists (arrays,
vectors, sequences ...) of base types such as strings, numbers and boolean
values. For a complete description you may refer to the complete reference at
json.org.

The configuration of a filter setup is stored in a JSON-encoded text file with a
``.json`` suffix. The root object must at least contain one ``"type"`` and one
``"elements"`` mapping. It may also define several ``"prop-set"`` s for further
reference.


Node Types
----------

A node type is specified by the key ``"type"`` in a JSON object::
 
  { "type" : $TYPE }

where ``$TYPE`` could be

``"sequence"``, that is a sequential chain of filters and containers::

    [filter A] --> [sequence S --> [filter X] -> [filter Y]]

``"split"``, a number of filters and containers working on data in truly parallel
fashion::

                          +--> [filter X] --+
  [filter A] --> [split] -+                 +-->
                          +--> [filter Y] --+

``"filter"``, a node that doesn't contain any other containers but specifies the
kind of computation.


Elements
--------

An ``"elements"`` key lists the children of a container type as an JSON array.
The children could be container types again or Filter specifications.


Split Containers
----------------

A split takes an input buffer and distributes it in specified order to its child
filters or containers.

Properties:

- ``"type"`` [string]: Must be ``"split"``.
- ``"mode"`` [string]: Mode of operation in which work should be distributed.
  The following string values are possible:
  
  - ``"random"``: choose next free child
  - ``"round-robin"``: push data to one child after another
  - ``"copy"``: copy the input and distribute it to `all` children

- ``"repeat"`` [integer]: Controls how many instances of the first filter should be
  created. If not specified it defaults to ``1``.


Sequence Containers
-------------------

Properties:

- ``"type"`` [string]: Must be ``"sequence"``.


Property Sets
-------------

To avoid to list the same properties for different filters over and over again,
properties can be pre-defined with a singular ``"prop-sets"`` mapping. Each key
is a name that can be referenced in filter nodes using the ``"prop-refs"``
array. The values are ordinary ``"property"`` mappings.


Filters
-------

A filter consists at least of a ``"plugin"`` key naming the filter that is going
to be used. Of course, plugins have to be available as a shared object in UFO's
path.

Properties:

- ``"type"`` [string]: Must be ``"filter"``.
- ``"plugin"`` [string]: Name of the associated plugin corresponding to the
  filter ``libfilterxxx.so``.
- ``"properties"`` [dict]: Mapping from string to value. Exact names and value
  types depend on the actual filter.
- ``"prop-refs"`` [array of strings]: Names of property sets that have been
  pre-defined with a ``"prop-sets"`` map.


Example
-------

Let's say we want to generate sinograms from a directory full of tomographic
projections. A suitable configuration would look like this::

    {
        "type" : "sequence",
        "elements" : [
            {
                "type" : "filter",
                "plugin" : "reader",
                "properties" : {
                    "path" : "/home/path/to/projections/",
                    "count" : 300
                }
            },
            {
                "type" : "filter",
                "plugin" : "sinogenerator",
                "properties" : {
                    "num-projections" : 300
                }
            },
            {
                "type" : "filter",
                "plugin" : "writer",
                "properties" : {
                    "path" : "/home/path/to/sinograms/",
                    "prefix" : "sino"
                }
            }
        ]
    }

Okay, now we have some sinograms. Using the following configuration we can
easily do an un-filtered back-projection ::

    {
        "type" : "sequence",
        "elements" : [
            {
                "type" : "filter",
                "plugin" : "reader",
                "properties" : {
                    "path" : "/home/path/to/sinograms/",
                    "count" : 200,
                    "prefix" : "sino"
                }
            },
            {
                "type" : "filter",
                "plugin" : "backproject",
                "properties" : {
                    "axis-pos" : 413.0,
                    "angle-step" : 0.01256637
                }
            },
            {
                "type" : "filter",
                "plugin" : "writer",
                "properties" : {
                    "path" : "/home/path/to/slices",
                    "prefix" : "slice"
                }
            }
        ]
    }

As you might correctly conclude, we could have also merged these two
configurations and skip writing the sinograms to disk.

.. _JSON: http://json.org
