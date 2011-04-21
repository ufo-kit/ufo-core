JSON Configuration Format
=========================

JSON_ is a self-contained, human-readable data-interchange format. It is pure
text and language independent. The main structures are key/value pairs
(hash-tables, dictionaries, associative arrays ...) and ordered lists (arrays,
vectors, sequences ...). For a complete description you may refer to the complete
reference at json.org.

The configuration of a filter setup is stored in a JSON-encoded text file with a
``.json`` suffix. The root object must at least contain one ``"type"`` and one
``"elements"`` mapping.


Node Types
----------

A node type is specified by the key "type" in a JSON object::
 
  { "type" : $TYPE }

where $TYPE could be

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

An ``"elements"`` key specifies the children of a container type as an JSON array.


Split Containers
----------------

A split takes an input buffer and distributes it in specified order to its child
filters or containers.

Properties:

- ``"mode"``: Specifies the mode of operation in which work should be
  distributed.  Possible modes are ``"random"``, ``"round-robin"`` and
  ``"copy"``.


Filters
-------

A filter consists at least of a ``"plugin"`` key naming the filter that is going
to be used. Of course, plugins have to be available as a shared object in UFO's
path.

Properties:

- ``"plugin"``: Name of the associated plugin

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
