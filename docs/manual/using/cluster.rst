.. _using-cluster:

==========================
Running tasks in a cluster
==========================

The UFO framework comes with built-in cluster capabilities based on ZeroMQ 3.2.
Contrary to bulk cluster approaches (e.g. solving large linear systems), UFO
tries to distribute `streamed` data on a set of multiple machines. On each
remote slave, ``ufod`` must be started. By default, the server binds to port
5555 on any available network adapter. To change this, use the ``-l/--listen``
option::
    
    $ ufod --listen tcp://ib0:5555

will let ``ufod`` use the first Infiniband-over-IP connection.

On the master host, you pass the remote slave addresses to the scheduler object.
In Python this would look like this::

    sched = Ufo.Scheduler(remotes=['tcp://foo.bar.org:5555'])

Address are notated according to `ZeroMQ <http://api.zeromq.org/3-2:zmq-tcp>`_.


Streaming vs. replication
=========================

Work can be executed in two ways: `streaming`, which means data is transferred
from a master machine to all slaves and returned to the master after computation
is finished and `replicated` in which each slaves works on its own subset of the
initial input data. The former must be used if the length of the stream is
unknown before execution, otherwise the stream could not be split up into equal
partitions.

Initially, the scheduler is set to streaming mode. To switch to replication
mode, you have to prepare the scheduler::

    sched = Ufo.Scheduler(remotes=remotes)
    sched.set_remote_mode(Ufo.RemoteMode.REPLICATE)
    sched.run(graph)

Using MPI
=========================
UFO has support for executing in cluster environments using MPI. See the
:ref:`section on MPI <using-mpi>`
