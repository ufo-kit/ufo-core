.. _using-ufo:

=========
Using UFO
=========

UFO is a framework for high-speed image processing at Synchrotron_ beamlines. It
facilitates every available hardware device to process tomographic data as fast
as possible with on-line reconstruction as the ultimate goal.

.. _Synchrotron: http://en.wikipedia.org/wiki/Synchrotron

It is written in C using the GLib_ and GObject_ libraries to provide an
object-oriented :ref:`API <ufo-api>`.

.. _GLib: http://developer.gnome.org/glib/
.. _GObject: http://developer.gnome.org/gobject/stable/index.html

After :ref:`installing <installation>` the framework you're ready to build your
own image processing pipeline.


Hello World
===========

The easiest UFO program in C looks like this::

    int main(void)
    {
        g_type_init();  /* you _must_ call this! */

        UfoGraph *graph = ufo_graph_new_from_json("hello-world.json");
        ufo_graph_run(graph);
        return 0;
    }
    
As you can see we simply construct a new UfoGraph object from a JSON encoded
:ref:`configuration file <json-configuration>` and execute that computation
pipeline.


Language Bindings
=================

There are no plans to support any languages with manually written language
bindings. However, UFO is a GObject-based library from which ``gir`` (GObject
Introspection) files can be generated at build time. Any language that supports
GObject Introspection and the ``gir``/``typelib`` format is thus able to
integrate UFO. No manual intervention is need if the GObject Introspection tools
are found.

Using GObject Introspection
---------------------------

Because several languages support GObject Introspection, you have to consult the
appropriate reference manuals to find out how the GObjects are mapped to their
language equivalents. Some of the options are

- Python: PyGObject_
- Javascript: Gjs_ and Seed_
- Vala has direct support using the ``--pkg`` option

.. _PyGObject: http://live.gnome.org/PyGObject
.. _Gjs: http://live.gnome.org/Gjs
.. _Seed: http://live.gnome.org/Seed

A `GNOME wiki page`__ lists all available runtime bindings. A small example
written in Javascript and to be used with Gjs can be found in
``core/tests/test.js`` directory.

__ http://live.gnome.org/GObjectIntrospection/Users

A simple Python example -- with Python-GObject installed -- would look like
this::

    from gi.repository import Ufo
    graph = Ufo.Graph()
    graph.read_from_json("some-graph.json")
    graph.run()
    
    
Multithreading for data parallelism
===================================

Each filter is executed in its own thread thus allowing some form of implicit
task parallelism. This property is especially useful as image processing
pipelines usually consist of a mixture of I/O and CPU bound filters
(readers/writers vs. actual computation).

On the other hand, if you intend to increase throughput with data parallelism,
you can do so in two ways. The easiest is to connect several instances in the
same way like this::

    reader = graph.get_filter('reader')
    long_computation1 = graph.get_filter('long_computation')
    long_computation2 = graph.get_filter('long_computation')
    reader.connect_to(long_computation1)
    reader.connect_to(long_computation2)
    
This is totally fine if you don't expect the final order to be the same as the
one from the reader. If you have to be sure that the order is correct, you can
use the mux/demux filter pair::

    reader.connect_to(demux)
    demux.connect_by_name('output1', long_computation1, 'default')
    demux.connect_by_name('output2', long_computation2, 'default')
    long_computation1.connect_by_name('default', mux, 'input1')
    long_computation2.connect_by_name('default', mux, 'input2')
    
    
Examples
========

Global shift detection using image correlation
----------------------------------------------

A shift in vertical and horizontal direction of two images in a sequence can be
estimated as the largest argument of a cross-correlation between those images. We
can improve performance by employing a transform into the Fourier domain first
and multiply the frequencies as in 

.. math::

    \textrm{argmax}\ \mathcal{F}^{-1}\{\mathcal{F}\{I_1\} \cdot (\mathcal{F}\{I_2\})^\star \}.
    
To compute the image correlation between all pairs in the image sequence, we
have to set the ``copy`` parameter of the ``demux`` filter to ``delayed``, which
means, that for input sequence :math:`S = (I_1, \ldots, I_n)`, output 1
transmits sequence :math:`S_1 = (I_1, \ldots, I_{n-1})` and output 2 transmits
sequence :math:`S_2 = (I_2, \ldots, I_{n})`. The code could look like this::

    from gi.repository import Ufo
    
    g = Ufo.Graph()
    
    # instantiate the filters and add them as global names
    m = globals()
    filters = ['reader', 'writer', 'normalize', 'demux', 'fft', 'ifft', 'argmax']
    for f in filters:
        m[f] = g.get_filter(f)

    # here we want to choose the names on our own
    cmul = g.get_filter('complex')
    conj = g.get_filter('complex')

    # choose path to read and write
    reader.set_properties(path='/home/user/data/')
    writer.set_properties(path='/home/user/data/processed')
    
    demux.set_properties(copy='delayed')
    fft.set_properties(dimensions=2)
    ifft.set_properties(dimensions=2)
    cmul.set_properties(operation='mul')
    conj.set_properties(operation='conj')

    reader.connect_to(normalize)
    normalize.connect_to(fft)
    fft.connect_to(demux)
    
    # First argument of complex multiplication is kept intact
    demux.connect_by_name('output1', cmul, 'input1')
    # Second argument must be conjugated ...
    demux.connect_by_name('output2', conj, 'default')
    # ... and then multiplied
    conj.connect_by_name('default', cmul, 'input2')
    cmul.connect_to(ifft)
    
    # Return to spatial representation and look for maximum argument
    ifft.connect_to(argmax)
    argmax.connect_to(writer)

    g.run()


Controlling debug messages
--------------------------

Generally, UFO emits debug messages under the log domain ``Ufo``. To handle
these messages you must set a log handler_ that decides what to do with the
messages. To ignore all messages in Python, you would have to write something
like ::

    from gi.repository import Ufo, GLib

    def ignore_message(domain, level, message, user):
        pass

    if __name__ == '__main__':
        GLib.log_set_handler("Ufo", GLib.LogLevelFlags.LEVEL_MASK,
            ignore_message, None)

.. _handler: http://developer.gnome.org/glib/unstable/glib-Message-Logging.html#g-log-set-handler

