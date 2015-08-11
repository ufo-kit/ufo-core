.. _installation-docker:

########################
Installation with Docker
########################

==============
Install Docker
==============

See `Docker documentation <https://docs.docker.com/installation/>`_ to install Docker on your system.

================
Build Dockerfile
================

Download `AMD-APP-SDK-v3.0-0.113.50-Beta-linux64 <http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/>`_, paste it to the directory with Dockerfile in it and run::

    $ docker build -t docker .
    
This will build the image of opensuse 13.1, install all dependencies, AMD drivers, ufo-core, ufo-filters and ufo-ir on it. 
    
==========
Use Docker
==========

Run installed image in a new Docker container and use gpu in it::

    $ docker run -i -t --device=/dev/ati/card0 docker /bin/bash
    
See all created containers::

    $ docker ps
    
See all created images::

    $ docker images
    
Stop a running container::

    $ docker stop #ID_or_container_name
    
Start a stopped container::

    $ docker start #ID_or_container_name
    
Attach to a running container::

    $ docker attach #ID_or_container_name
    
For more help run::

    $ docker --help

If you need the container IP, call in a running container::

    $ cd etc
    $ vi hosts
