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


Profiling
=========

By default, the scheduler measures the run-time between from initial setup to
the processing of the last data item. You can get the time in seconds via the
``time`` property ::

    g = Ufo.TaskGraph()
    scheduler = Ufo.Scheduler()
    scheduler.run(g)
    print("Time spent: {}s".format(scheduler.time))

To get more fine-grained insight into the execution, you can enable tracing ::

    scheduler.props.enable_tracing = True
    scheduler.run(g)

which will generate traces for OpenCL (saved in ``opencl.PID.json``) and general
events (saved in ``trace.PID.json``). To visualize the trace events, you can use
the Google Chrome or Chromium browser, go to chrome://tracing and load the JSON
files.
