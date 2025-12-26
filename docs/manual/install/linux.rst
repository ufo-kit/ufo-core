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
    centos $ wget <URL>/home:ufo-kit.repo -O /etc/yum.repos.d/ufo-kit.repo

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


.. _ubuntu22.04:

Ubuntu/Debian (Tested on Ubuntu 22.04.3 LTS)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Debian or Debian-based system the following packages are required to build
ufo-core::

    $ apt install build-essential cmake meson libglib2.0-dev libjson-glib-dev

In case you want to use UFO with NVIDIA cards, you need to install the driver
and CUDA. It also makes sense to change the
`/lib/systemd/system/nvidia-persistenced.service` persistence option from
`--no-persistence-mode` to `--persistence-mode`. This will speed up the
initialization of the cards. CUDA installation (package versions may of course
quickly change) ::

    $ apt install nvidia-driver-550 nvidia-utils-550 nvidia-cuda-toolkit

You will also need an OpenCL ICD loader. To simply get the build running, you
can install ::

    $ apt install ocl-icd-opencl-dev

Generating the introspection files for interfacing with third-party languages
such as Python you must install ::

    $ apt install gobject-introspection libgirepository1.0-dev

and advised to install ::

    $ apt install python3-dev python3-pip

To use the ``ufo-mkfilter`` script you also need the jinja2 Python package::

    $ apt install python3-jinja2

Building the reference documentation and the Sphinx manual requires::

    $ apt install gtk-doc-tools python3-sphinx python3-sphinxcontrib.bibtex 
    $ pip install sphinx_rtd_theme

Additionally the following packages are recommended for some of the plugins::

    $ apt install libtiff5-dev


openSUSE and CentOS7
~~~~~~~~~~~~~~~~~~~~

For openSUSE (zypper) and CentOS7 the following packages should get you started::

    $ [zypper|yum] install cmake gcc gcc-c++ glib2-devel json-glib-devel

Additionally the following packages are recommended for some of the plugins::

    $ [zypper|yum] install libtiff-devel


Building ufo-core with CMake
----------------------------

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

If everything went well, you can install the library with ::

  $ make install

.. seealso:: :ref:`faq-linker-cant-find-libufo`

To run and build the tests do ::

  $ cmake -DWITH_TESTS=ON <path-to-ufo>
  $ make test

Building ufo-core with meson
----------------------------

Configure the build with ``meson`` by changing into the root source directory
and type ::

  $ meson build

You can change the location of GNU installation directories during this step or
later with the ``meson configure`` tool ::

  $ meson build --prefix=/usr
  $ cd build && meson configure -Dprefix=/usr/local

Build, test and install everything with ::

  $ cd build
  $ ninja
  $ ninja install

Building and running the tests ::

  $ cd build
  $ ninja configure -Dwith_tests=true
  $ ninja test


Building ufo-filters
--------------------

Once ufo-core is installed you can build the filter suite in a pretty similar
way using cmake ::

    $ mkdir -p build/ufo-filters
    $ cd build/ufo-filters
    $ cmake <path-to-ufo-filters> -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64
    $ make
    $ make install

or using meson ::

  $ meson build --libdir=/usr/local/lib -Dcontrib_filters=True
  $ cd build
  $ ninja
  $ ninja install


Python support
--------------

ufo-core has GObject introspection to let third-party languages interface with
the library. To build the support files you need the GObject introspection
scanner ``g-ir-scanner`` and compiler ``g-ir-compiler`` which you can get on
Ubuntu via ::

    $ apt install python-gi-dev

In the ``python/`` subdirectory of the source distribution, additional Python
modules to interface more easily with the framework is provided. To install the
NumPy module and the high-level interface run ::

    $ cd python/ && python3 setup install


Post-install Steps
------------------

To install the ufo-tofu software stack with their full capabilities it is required to set some
environment variables to resolve package dependencies. We need to make sure that ``pkg-config``
package in available and there are mainly three environment variables to take care. To set these
environment variables correctly we may refer to the install logs for meson or cmake. For meson
this log is generally available in the ``BUILD_DIR/meson-logs/install-logs.txt``.

In the install logs we need to specifically look at

* line that gives us the location of the ``ufo.pc`` file for the pkg-config reference.
* line that gives us the location of the ``Ufo-<version>.typelib`` file of gi-repository.

e.g., ::

   $HOME/.local/lib/x86_64-linux-gnu/pkgconfig/ufo.pc
   $HOME/.local/lib/x86_64-linux-gnu/girepository-1.0/Ufo-0.0.typelib

We need to use these information to set the following environment variables in the manner depicted
in the following examples ::

  $ export export PKG_CONFIG_PATH=$HOME/.local/lib/x86_64-linux-gnu/pkgconfig
  $ export GI_TYPELIB_PATH=$HOME/.local/lib/x86_64-linux-gnu/girepository-1.0
  $ export LD_LIBRARY_PATH=$HOME/.local/lib/x86_64-linux-gnu

This example assumes that while installing the ufo we specified ``$HOME/.local`` as the install
prefix e.g., ::

  $ meson build --prefix=~/.local
  $ cd build && meson configure -Dprefix=~/.local

In absence of these environment variables set correctly we are likely to have the following error
while using ufo-tofu software stack from command line ::

  ValueError: Namespace Ufo not available

In this situation one way to verify that the required environment variables are correctly set is to
query ``pkg-config`` for the package name ``ufo`` ::

  $ pkg-config --validate ufo

which is like to to yield the following message ::

  Package ufo was not found in the pkg-config search path.
  Perhaps you should add the directory containing `ufo.pc'
  to the PKG_CONFIG_PATH environment variable
  No package 'ufo' found

One can also try to look up the package ufo in the list of packages ::

  $ pkg-config --list-all

which will list down all system packages, which are accounted for and ufo must be in the list for
ufo-tofu software stack to function properly. Once the packages are correctly set ufo should appear
in the list of packages and validation should not yield any error.

It is possible to install ufo-tofu software stack with full capabilities inside container images and
for that setting these environment variables correctly is crucial.

Refer to the README for additional information.