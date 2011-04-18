UFO's Application Programming Interface
=======================================

Elements
--------

An UFO computation graph consists of leaf elements which perform actual
computation or of containers like Splits and Sequences which contain child
elements.

.. doxygenclass:: UfoElement
    :project: ufo


.. doxygenfunction:: ufo_element_new
    :project: ufo


Buffers
-------

Buffers are used to pass data from one filter to the other. To do so a filter
pops a buffer from its internal input_queue and pushes a newly requested or the
altered buffer to its output_queue.

.. doxygenclass:: UfoBuffer
    :project: ufo

.. doxygenfunction:: ufo_buffer_new
    :project: ufo

.. doxygenfunction:: ufo_buffer_reinterpret
    :project: ufo


Resource Manager
----------------
