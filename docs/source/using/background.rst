.. _using-objects:

====================
Technical Background
====================

Relationship between graph and scheduler
========================================

A ``Ufo.Graph`` represents a network of interconnected filter nodes. New nodes
can be added and existing node relationships be queried. Also, the graph can be
serialized as a JSON structure with ``ufo_graph_save_to_json`` and read back
again with ``ufo_graph_read_from_json``.

The ``Ufo.Scheduler`` on the other hand is an implementation of a strategy *how*
to execute the filters contained in a graph. Therefore, the scheduler is passed
a graph object on execution.

Configuration
=============

There are two different notions of configuration in the Ufo framework: per-node
configuration and execution configuration. The former is realized with GObject
properties. In Python, these properties can be set as a named parameter with
``set_properties`` or assigned to the property as part of the ``props``::

    writer = pm.get_filter('reader')

    writer.props.prefix = 'foo'
    writer.set_properties(prefix='foo')

The execution configuration is independent of the parameters of the actual
computation and used to determine environment specific foos. For example, if the
filters are not installed system wide, there need to be a way to tell the
framework were these are located. This information is stored in an
``Ufo.Configuration`` object. Each part of the framework that implements the
``Ufo.Configurable`` interface accepts such an object at construction time and
uses necessary information stored within::

    # Lets assume that filters and .cl files are stored in the parent directory.
    # So we create a new configuration object and set its `paths' property.
    config = Ufo.Configuration(paths=['..'])

    # The PluginManager is configurable ...
    pm = Ufo.PluginManager(configuration=config)

    # ... so is the scheduler
    scheduler = Ufo.Scheduler(configuration=config)


Profiling
=========

Profiling is disabled by default but can be enabled with the ``profile-level``
property of a configuration object. This property receives values from the
``UfoProfilerLevel`` flags enum ::

    # track only OpenCL events
    config = Ufo.Configuration(profile_level=Ufo.ProfilerLevel.OPENCL)

    scheduler = Ufo.Scheduler(configuration=config)

If you do not specify an output file name (``profile_output`` property), the
profiling information is output to ``stdout``.

The profiling information can be analysed with the ``clprof`` tool, as part of
the standard distribution::

    $ clprof stats
    Kernel               Submit Delay   Exec Delay  Kernel Exec   Total Exec   Queue Dist
    -------------------------------------------------------------------------------------
    filter                     0.0033       1.0551       0.0491       0.4915          1.0
    fft_spread                 0.0079       1.0265       0.0398       0.3982          1.0
    backproject_tex            0.0022       0.5808       4.9480      49.4805          1.0
    fft_pack                   0.0034       0.1187       0.0311       0.3113          1.0

The output is the averaged time in milli seconds for submission delay, execution
delay and kernel execution:

* *Submission delay*: time between calling the kernel and actually submission
  into the command queue
* *Execution delay*: time between enqueueing and execution of the kernel
* *Execution*: time for executing the kernel

Moreover, the total execution time in milli seconds and the kernel distribution
among the command queues is shown.
