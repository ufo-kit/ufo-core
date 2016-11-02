.. _faq:

==========================
Frequently Asked Questions
==========================

Installation
============

.. _faq-linker-cant-find-libufo:

Why can't the linker find libufo.so?
----------------------------------------

In the rare circumstances that you installed UFO from source for the first time
by calling ``make install``, the dynamic linker does not know that the library
exists. If this is the case issue ::

  $ sudo ldconfig

on Debian systems or ::

  $ su
  $ ldconfig

on openSUSE systems.

If this is not working, the library is neither installed into ``/usr/lib`` nor
``/usr/local/lib`` on 32-bit systems or ``/usr/lib64`` and ``/usr/local/lib64``
on 64-bit systems. Make sure not to mess with the ``CMAKE_INSTALL_PREFIX``
during source :ref:`configuration <inst-installing-into-non-standard-directories>`.


Usage
=====

.. _faq-filter-not-found:

Why do I get a "libfilter<foo>.so not found" message?
-------------------------------------------------------

Because the UFO core system is unable to locate the filters. By default it looks
into ``${LIBDIR}/ufo``. If you don't want to install the filters system-wide,
you can tell the system to try other paths as well by appending paths to the
``UFO_PLUGIN_PATHS`` environment variable.


Can I split a linear data stream?
---------------------------------

The output data stream of a node can be split by setting the
``UFO_SEND_SEQUENTIAL`` mode and adjusting the number of expecting data items on
each connected node::

    from gi.repository import Ufo

    out_node.set_send_pattern(Ufo.SendPattern.SEQUENTIAL)
    in1_node.set_num_expected(0, 5)  # expect five items on the first input
    in2_node.set_num_expected(0, -1) # expect all items

    g = Ufo.TaskGraph()
    g.connect_nodes(out_node, in1_node)
    g.connect_nodes(out_node, in2_node)

The connection order matters here! If it would be reversed, ``in2_node`` would
receive all items whereas ``in1_node`` wouldn't receive anything.


How can I control the debug output from libufo?
-----------------------------------------------

Generally, UFO emits debug messages under the log domain ``Ufo``. If you use a
UFO-based tool and cannot see debug messages, you might have to enable them by
setting the ``G_MESSAGES_DEBUG`` environment variable, i.e.::

    export G_MESSAGES_DEBUG=Ufo

To handle these messages from within a script or program, you must set a log
handler_ that decides what to do with the messages. To ignore all messages in
Python, you would have to write something like this::

    from gi.repository import Ufo, GLib

    def ignore_message(domain, level, message, user):
        pass

    if __name__ == '__main__':
        GLib.log_set_handler("Ufo", GLib.LogLevelFlags.LEVEL_MASK,
            ignore_message, None)

.. _handler: http://developer.gnome.org/glib/unstable/glib-Message-Logging.html#g-log-set-handler


.. _faq-numpy-output:

How can I use Numpy output?
---------------------------

Install the ``ufo-python-tools``.  You can then use the BufferInput filter to
process Numpy arrays data::

    from gi.repository import Ufo
    import ufo.numpy
    import numpy as np

    arrays = [ i*np.eye(100, dtype=np.float32) for i in range(1, 10) ]
    buffers = [ ufo.numpy.fromarray(a) for a in arrays ]

    pm = Ufo.PluginManager()
    numpy_input = pm.get_task('bufferinput')
    numpy_input.set_properties(buffers=buffers)


.. _faq-synchronize-properties:

How can I synchronize two properties?
-------------------------------------

Although this is a general GObject question, synchronizing two properties is
particularly important if the receiving filter depends on a changed property.
For example, the back-projection should start only if a center-of-rotation is
known. In Python you can use the ``bind_property`` function from the
``ufotools`` module like this::

    from gi.repository import Ufo
    import ufotools.bind_property

    pm = Ufo.PluginManager()
    cor = g.get_task('centerofrotation')
    bp = g.get_task('backproject')

    # Now connect the properties
    ufotools.bind_property(cor, 'center', bp, 'axis-pos')

In C, the similar ``g_object_bind_property`` function is provided out-of-the-box.
