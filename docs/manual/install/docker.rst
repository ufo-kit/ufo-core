.. _installation-docker:

########################
Installation with Docker
########################

Before, proceeding have a look at the `Docker documentation
<https://docs.docker.com/engine/installation>`_ and install Docker on your
system.


==================
Rady-to-use Images
==================

You can pull the following images from the `ufo-kit
<https://hub.docker.com/r/tfarago/ufo-kit/>`_ repository on dockerhub without
the need of `Build`_\ing them::

    docker pull tfarago/ufo-kit:ufo-nvidia-ubuntu-20.04
    docker pull tfarago/ufo-kit:ufo-intel-ubuntu-20.04

=====
Build
=====

Depending on the GPUs in your system you have to use a different Dockerfile.
Before doing so, create an empty directory and copy the respective Dockerfile
from the ``docker`` directory and rename it to ``Dockerfile``. In the case of
the AMD-based Dockerfile you have to download
`AMD-APP-SDK-v3.0-0.113.50-Beta-linux64
<http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/>`_,
and move it into the same directory. Now go into the directory and type::

    $ docker build -t ufo .
    
This will build an image tagged ``ufo`` containing both ufo-core and
ufo-filters.
    
============
Usage on AMD
============

Run the image in a new Docker container using::

    $ docker run -it --device=/dev/ati/card0 ufo
    
===============
Usage on NVIDIA
===============

Install `nvidia-container-runtime
<https://nvidia.github.io/nvidia-container-runtime/>`_ and then pull the image::

    $ docker pull tfarago/ufo-kit:ufo-nvidia-ubuntu-20.04

and run::

    $ docker run --rm -it --gpus all tfarago/ufo-kit:ufo-nvidia-ubuntu-20.04

If you want to use the graphical user interfaces (GUIs) run::

    $ sudo xhost +local:username
    $ docker run --rm -it --gpus all -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY tfarago/ufo-kit:ufo-nvidia-ubuntu-20.04


===============
Usage on Intel
===============

First install the OpenCL runtime (Ubuntu-specific but it should be similar for
other distributions)::

    $ sudo apt-get install intel-opencl-icd

then pull the image::

    $ docker pull tfarago/ufo-kit:ufo-intel-ubuntu-20.04

and run the following::

    $ docker run --rm -it --device /dev/dri:/dev/dri tfarago/ufo-kit:ufo-intel-ubuntu-20.04

or this for GUI support::

    $ sudo xhost +local:username
    $ docker run --rm -it --device /dev/dri:/dev/dri -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY tfarago/ufo-kit:ufo-intel-ubuntu-20.04
