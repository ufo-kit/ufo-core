==============
Task execution
==============

This section provides a deeper look into the technical background concerning
scheduling and task execution. The execution model of the UFO framework is based
on the ``Ufo.TaskGraph`` that represents a network of interconnected task
nodes and the ``Ufo.BaseScheduler`` that runs these tasks according to a
pre-defined strategy. The ``Ufo.Scheduler`` is a concrete implementation and is
the default choice because it is able to instantiate tasks in a multi-GPU
environment. For greater flexibility, the ``Ufo.FixedScheduler`` can be used to
define arbitrary GPU mappings.


Profiling execution
===================

By default, the scheduler measures the run-time from initial setup until
processing of the last data item finished. You can get the time in seconds via the
``time`` property ::

    g = Ufo.TaskGraph()
    scheduler = Ufo.Scheduler()
    scheduler.run(g)
    print("Time spent: {}s".format(scheduler.time))

To get more fine-grained insight into the execution, you can enable tracing ::

    scheduler.props.enable_tracing = True
    scheduler.run(g)

and analyse the generated traces for OpenCL (saved in ``opencl.PID.json``) and
general events (saved in ``trace.PID.json``). To visualize the trace events, you
can either use the distributed ``ufo-prof`` tool or Google Chrome or Chromium by
going to chrome://tracing and loading the JSON files.


Broadcasting results
====================

.. highlight:: python

Connecting a task output to multiple consumers will in most cases cause
undefined results because some data is processed differently than others. A
certain class of problems can be solved by inserting explicit ``Ufo.CopyTask``
nodes and executing the graph with a ``Ufo.FixedScheduler``. In the following
example, we want write the same data twice with a different prefix::

    from gi.repository import Ufo

    pm = Ufo.PluginManager()
    sched = Ufo.FixedScheduler()
    graph = Ufo.TaskGraph()
    copy = Ufo.CopyTask()

    data = pm.get_task('read')

    write1 = pm.get_task('write')
    write1.set_properties(filename='w1-%05i.tif')

    write2 = pm.get_task('write')
    write2.set_properties(filename='w2-%05i.tif')

    graph.connect_nodes(data, copy)
    graph.connect_nodes(copy, write1)
    graph.connect_nodes(copy, write2)

    sched.run(graph)

.. note:: 

    The copy task node is not a regular plugin but part of the core API and
    thus cannot be used with tools like ``ufo-runjson`` or ``ufo-launch``. 
