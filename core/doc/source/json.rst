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
``"elements"`` mapping.


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

- ``"mode"`` [string]: Mode of operation in which work should be distributed.
  The following string values are possible:
  
  - ``"random"``: choose next free child
  - ``"round-robin"``: push data to one child after another
  - ``"copy"``: copy the input and distribute it to `all` children

- ``"repeat"`` [integer]: Controls how many instances of the first filter should be
  created. If not specified it defaults to ``1``.


Filters
-------

A filter consists at least of a ``"plugin"`` key naming the filter that is going
to be used. Of course, plugins have to be available as a shared object in UFO's
path.

Properties:

- ``"plugin"`` [string]: Name of the associated plugin corresponding to the
  filter ``libfilterxxx.so``.
- ``"properties"`` [dict]: Mapping from string to value. Exact names and value
  types depend on the actual filter


Example
-------

An example configuration would look like this::

    {
        "type" : "sequence",
        "elements" : [
            {
                "type" : "filter",
                "plugin" : "uca"
            },
            {
                "type" : "filter",
                "plugin" : "scale",
                "properties" : {
                    "scale" : 0.5
                }
            },
            {
                "type" : "split",
                "mode" : "copy",
                "elements" : [
                    {
                        "type" : "filter",
                        "plugin" : "raw"
                    },
                    {
                        "type" : "filter",
                        "plugin" : "histogram"
                    }
                ]
            }
        ]
    }


TODO
----

- how to specify merge in `split`

.. _JSON: http://json.org
