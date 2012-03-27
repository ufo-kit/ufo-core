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
into ``${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/ufo``, where the installation
prefix is usually something like ``/usr`` or ``/usr/local``. If you don't want
to install the filters system-wide, you can tell the system to try other paths
as well::

  >>> from gi.repository import Ufo
  >>> graph = Ufo.Graph(paths='/home/user/path/to/filters:/another/path')


How can I control the debug output from libufo?
-----------------------------------------------

Generally, UFO emits debug messages under the log domain ``Ufo``. To handle
these messages you must set a log handler_ that decides what to do with the
messages. To ignore all messages in Python, you would have to write something
like this::

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

Install the Python extension module from ``vogelgesang/ufonp`` with ``setup.py``
like this::

    $ cd <path-to-ufonp>
    $ python setup.py build
    $ sudo python setup.py install

You can then use the BufferInput filter to process Numpy arrays data::

    from gi.repository import Ufo
    import ufotools
    import numpy as np

    arrays = [ i*np.eye(100, dtype=np.float32) for i in range(1, 10) ]
    buffers = [ ufotools.fromarray(a) for a in arrays ]

    g = Ufo.Graph()
    numpy_input = g.get_filter('bufferinput')
    numpy_input.set_properties(buffers=buffers)


How can I instantiate and pass parameters when creating a filter?
-----------------------------------------------------------------

Yes, the same module that is used to access Numpy buffers has a convenience
wrapper around the :c:type:`UfoGraph` class that provides a ``new_filter`` method::

    import ufotools.patch

    g = ufotools.patch.Graph()
    rd = g.new_filter('reader', path='/home/src', count=5)
    wr = g.new_filter('writer', path='/home/dst', prefix='foo-')


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

    g = Ufo.Graph()
    cor = g.get_filter('centerofrotation')
    bp = g.get_filter('backproject')

    # Now connect the properties
    ufotools.bind_property(cor, 'center', bp, 'axis-pos')

In C, the similar ``g_object_bind_property`` function is provided out-of-the-box.
