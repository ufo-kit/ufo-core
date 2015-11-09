.. _using-details:

==============
Detailed usage
==============

Environment variables
=====================

You can modify the run-time behaviour by setting environment variables:

.. envvar:: G_MESSAGES_DEBUG

    Controls the output of the library. By default nothing is printed on stdout.
    Set this to `all` to see debug output.

.. envvar:: UFO_DEVICES

    Controls which OpenCL devices should be used. It works similar to the
    `CUDA_VISIBLE_DEVICES` environment variables, i.e. set it to `0,2` to choose
    the first and third device that's available.
