.. _ufo-api:

=======================================
UFO's Application Programming Interface
=======================================

Overview
========

UFOs main purpose is to build streaming setups for fast image processing. To
describe these setups there are three important abstract definitions: `Split`_,
`Sequence`_ and `Filters`_. Due to the fact, that GObject only allows single
inheritance we have to make the following important observation: an `Element`
interface provides a `process` method which is implemented only by `Filters`,
`Sequences` and `Splits`. `Sequences` and `Splits` are sub-classes of
`Containers`_ which contain further `Elements`. 

Each `Element` must implement the `process` method. The `process` behaviour of a
filter is clear: it may process some input and may generate some output. The
main purpose of `process` for `Sequences` and `Splits` is to start its children
and distribute incoming work.

The main entry point for the whole computation is a `Graph`_ which holds a
reference to the very first root `Container`.


Elements
========

Defined in ``ufo-element.h``

.. doxygenclass:: UfoElement
    :project: ufo
    :members: 

Filters
-------

.. doxygenclass:: UfoFilter
    :project: ufo
    :members:

Containers
----------

.. doxygenclass:: UfoContainer
    :project: ufo
    :members:

Sequence
~~~~~~~~

.. doxygenclass:: UfoSequence
    :project: ufo
    :members:


Split
~~~~~

.. doxygenclass:: UfoSplit
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

