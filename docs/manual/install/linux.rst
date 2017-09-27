.. _installation-linux:

#####################
Installation on Linux
#####################

Installing RPM packages
=======================

We provide RPM-based packages for openSUSE, RHEL and CentOS7. First you have to
add the openSUSE repository matching your installation. Go to our `OBS
<https://build.opensuse.org/repositories/home:ufo-kit>`_ page, copy the URL that
says "Go to download repository" and use that URL with::

    opensuse $ zypper addrepo <URL> repo-ufo-kit
    centos $ wget <URL> -O /etc/yum.repos.d/ufo-kit.repo

Now update the repositories and install the framework and plugins::

    opensuse $ zypper install ufo-core ufo-filters
    centos $ yum install ufo-core ufo-filters


Installing Debian packages
==========================

UFO is part of Debian Sid and thus also available since Ubuntu 17.04. To install
both the core framework and the filters install::

    $ apt install libufo-bin libufo-dev ufo-filters


Installing from source
======================

If you want to build the most recent development version, you have to clone the
source from our repository, install all required dependencies and compile the
source.


Retrieving the source code
--------------------------

In an empty directory, issue the following commands to retrieve the current
unstable version of the source::

    $ git clone https://github.com/ufo-kit/ufo-core
    $ git clone https://github.com/ufo-kit/ufo-filters

The latter is used for developers who have write-access to the corresponding
repositories. All stable versions are tagged. To see a list of all releases
issue::

    $ git tag -l


Installing dependencies
-----------------------

UFO has only a few hard source dependencies: `GLib 2.0
<http://developer.gnome.org/glib/stable/>`_, `JSON-GLib 1.0
<http://live.gnome.org/JsonGlib>`_ and a valid OpenCL installation.
Furthermore, it is necessary to build the framework with a recent version of
`CMake <http://cmake.org>`_. `Sphinx <http://sphinx.pocoo.org>`_ is used to
create this documentation. This gives you a bare minimum with reduced
functionality. To build all plugins, you also have to install dependencies
required by the plugins.

OpenCL development files must be installed in order to build UFO. However, we
cannot give general advices as installation procedures vary between different
vendors. However, our CMake build facility is in most cases intelligent enough
to find header files and libraries for NVIDIA CUDA and AMD APP SDKs.


Ubuntu/Debian
~~~~~~~~~~~~~

On Debian or Debian-based system the following packages are required to build
ufo-core::

    $ sudo apt-get install build-essential cmake libglib2.0-dev libjson-glib-dev

You will also need an OpenCL ICD loader. To simply get the build running, you
can install ::

    $ sudo apt-get install ocl-icd-opencl-dev

Generating the introspection files for interfacing with third-party languages
such as Python you must install ::

    $ sudo apt-get install gobject-introspection libgirepository1.0-dev

and advised to install ::

    $ sudo apt-get install python-dev

To use the ``ufo-mkfilter`` script you also need the jinja2 Python package::

    $ sudo apt-get install python-jinja2

Building the reference documentation and the Sphinx manual requires::

    $ sudo apt-get install gtk-doc-tools python-sphinx

Additionally the following packages are recommended for some of the plugins::

    $ sudo apt-get install libtiff5-dev


openSUSE and CentOS7
~~~~~~~~~~~~~~~~~~~~

For openSUSE (zypper) and CentOS7 the following packages should get you started::

    $ [zypper|yum] install cmake gcc gcc-c++ glib2-devel json-glib-devel

Additionally the following packages are recommended for some of the plugins::

    $ [zypper|yum] install libtiff-devel


Building ufo-core
-----------------

Change into another empty `build` directory and issue the following commands to
configure ::

  $ cmake <path-to-ufo>

CMake will notify you, if some of the dependencies are not met. In case you want
to install the library system-wide on a 64-bit machine you should generate the
Makefiles with ::

  $ cmake -DCMAKE_INSTALL_LIBDIR=lib64 <path-to-ufo>

For earlier versions of PyGObject, it is necessary that the introspection files
are located under ``/usr`` not ``/usr/local``. You can force the prefix by
calling ::

  $ cmake -DCMAKE_INSTALL_PREFIX=/usr <path-to-ufo>

Last but not least build the framework, introspection files, API reference and
the documentation using ::

  $ make

You should now run some basic tests with ::

  $ make test

If everything went well, you can install the library with ::

  $ make install

.. seealso:: :ref:`faq-linker-cant-find-libufo`


Building ufo-filters
--------------------

Once ufo-core is installed you can build the filter suite in a pretty similar
way ::

    $ mkdir -p build/ufo-filters
    $ cd build/ufo-filters
    $ cmake <path-to-ufo-filters> -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64
    $ make
    $ make install


Python support
--------------

ufo-core has GObject introspection to let third-party languages interface with
the library. To build the support files you need the GObject introspection
scanner ``g-ir-scanner`` and compiler ``g-ir-compiler`` which you can get on
openSUSE via ::

    $ zypper install gobject-introspection-devel python-gobject2

In the ``python/`` subdirectory of the source distribution, additional Python
modules to interface more easily with the framework is provided. To install the
NumPy module and the high-level interface run ::

    $ cd python/ && python setup install

Refer to the README for additional information.
