.. _using-mpi:

================
Running with MPI
================

UFO can run on a cluster system using MPI. This makes sense if lots of data is
sent over the network, as MPI can benefit from low-level access to the hardware
(such as InfiniBand) and needs not to rely on TCP/IP.

For problems with high compute-to-transfer ratio (that is if you have very long
running computation per work-item) using regular TCP/IP and UFOs daemon mode
with ufod might be equally fast and easier to setup. See Running on a cluster
for that.

MPI Requirements
================

UFO is tested against OpenMPI 1.6.5, but should work with other implementations.
It is important to notice that UFO requires the MPI_THREAD_SERIALIZED or higher
thread-level compiled into MPI, which is not the case for most Linux
distribution-default implemenations. Support for MPI_THREAD_SERIALIZED is also
most often not compiled in by default.

For OpenMPI you can enable the required thread level by passing the
--enable-threads flag to the configure script.

OpenMPI lets you check the supported thread levels::

	$ ompi_info | grep -i thread

You need support for MPI_THREAD_SERIALIZED or higher (i.e. MPI_THREAD_MULTIPLE).

**Note: It is not currently possible to use MPI with Python.**

Compiling UFO
=============

Support for MPI has to be compiled into UFO. This can be done by adding the
``-DWITH_MPI=ON`` flag (CMake) or ``--enable-mpi`` (autotools).

Running with MPI
================

It is assumed that you have installed UFO on all nodes and setup each node’s
environment to find the ufo binary, filters, and libraries. It is adviced to
install UFO onto a network share that is accesible from all nodes.

With OpenMPI, you need to create a list of hosts that should be used when
running in the cluster. A sample hosts file for OpenMPI can look like this::

	192.168.11.61
	192.168.11.62
	192.168.11.63


The first host/IP is always the central communication node. This should be the
system that you are logged in to and you use to start the UFO MPI process. The
other nodes are remote computation nodes that will do the actual computation.
You can also append the local machine to the list, in that case the local host
serves as compute node and receives data via loopback.

The central communication node forwards and collects all data from the remote
nodes, so the input data should be on the central node’s (fast) storage, or
devices to acquire data should be plugged into the central node.

To start the MPI process::

	$ mpirun {$UFO_PREFIX}/tools/ufo-runjson inputfile.json

Using OpenMPI, this could for example look like this::

	$ mpirun --hostfile hosts --tag-output /home/user/ufo/tools/ufo-runjson -p /home/user/ufo/ufo-filters/src/ inputfile.json
