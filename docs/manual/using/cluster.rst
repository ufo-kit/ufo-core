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
