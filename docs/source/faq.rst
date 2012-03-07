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


How can I use Numpy output?
---------------------------

Install the Python extension module from ``vogelgesang/ufonp`` with ``setup.py``
like this::

    $ cd <path-to-ufonp>
    $ python setup.py build
    $ sudo python setup.py install

You can then use the BufferInput filter to process Numpy arrays data::

    from gi.repository import Ufo
    import ufonp
    import numpy as np

    A = np.zeros((1024, 1024))
    b = Ufo.Buffer()
    ufonp.fromarray(b, A)

    g = Ufo.Graph()
    numpy_input = g.get_filter('bufferinput')
    numpy_input.set_properties(buffers=[b])
