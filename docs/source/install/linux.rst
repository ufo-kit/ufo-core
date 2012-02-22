.. _installation-linux:

#####################
Installation on Linux
#####################

=======================================
Installation from pre-compiled binaries
=======================================

Debian and RPM packages are provided. To install the Debian package under Ubuntu
or Debian, issue ::

  $ sudo dpkg -i libufo-x.y.deb

Unfortunately, the OpenCL distributions from NVIDIA and AMD are not known to the
repository system of openSUSE. When installing UFO, ``rpm`` will complain, that
it cannot find this dependency. To solve this problem use ``zypper`` ::

  $ sudo zypper install libufo-x.y.rpm

and when asked to fix the dependency, ignore it.


====================
Building from source
====================

UFO has only a few hard source dependencies, namely

  - `GLib 2.0 <http://developer.gnome.org/glib/stable/>`_, 
  - `JSON-GLib 1.0 <http://live.gnome.org/JsonGlib>`_ and
  - a valid OpenCL installation.

Furthermore it is convenient to build the framework and accompanying
documentation with a recent version of `CMake <http://cmake.org>`_, `Doxygen
<http://doxygen.org>`_ and `Sphinx <http://sphinx.pocoo.org>`_. The current
distributed version control system in use, is `Bazaar <bazaar.canonical.com>`_.

In case you use openSUSE, just issue ::

    $ zypper install gcc gcc-c++ glib2-devel json-glib-devel
    $ zypper install gobject-introspection-devel python-gobject2
    $ zypper install gtk-doc python-Sphinx
    $ zypper install libtiff-devel
    
to install all dependencies.


Building Dependencies
=====================

OpenCL development files must be installed in order to build UFO. However, we
cannot give general advices as installation procedures vary between different
vendors. However, our CMake build facility is in most cases intelligent enough
to find header files and libraries.


Checking out the Code
=====================

In an empty directory, issue the following commands to retrieve the current
unstable version of the source ::

  $ bzr clone bzr+ssh://<user>@ufo.kit.edu/vogelgesang/ufo-core

All stable versions are kept at ``ufo-core/ufo-core-x.y``.


Configuration and Compilation
=============================

Change into another empty `build` directory and issue the following commands to
configure ::

  $ cmake <path-to-ufo>

CMake will notify you, if some of the dependencies are not met. In case you want
to install the library system-wide on a 64-bit machine you should generate the
Makefiles with ::

  $ cmake <path-to-ufo> -DLIB_SUFFIX=64

You can adjust some build parameters later on by using the ``ccmake`` tool in
the build directory ::

  $ ccmake .

For earlier versions of PyGObject, it is necessary that the introspection files
are located under ``/usr`` not ``/usr/local``. You can force the prefix by
calling ::

  $ cmake <path-to-ufo> -DCMAKE_INSTALL_PREFIX=/usr

or change ``CMAKE_INSTALL_PREFIX`` variable with ``ccmake``.

Last but not least build the framework, introspection files, API reference and
the documentation using ::

  $ make

You can then proceed to build installation packages in ``RPM`` and ``DEB``
format by issueing ::

  $ make packages

or source tarballs with ::

  $ make packages_source

To install the library and `pkg-config` files, issue ::

  $ make install

.. seealso:: :ref:`faq-linker-cant-find-libufo`


.. _inst-installing-into-non-standard-directories:

Installing into non-standard directories
----------------------------------------

It is possible to install the library in a non-standard directory, for example
in the home directory of a user. In case we want to install in ``~/tmp/usr``, we
have to configure the project like this ::

  $ cmake <path-to-ufo> -DCMAKE_INSTALL_PREFIX=/home/user/tmp/usr

After building with ``make`` and installing into ``~/tmp/usr`` with ``make
install``, we have to adjust the ``pkg-config`` path, so that the library can be
found when configuring the filters ::

  $ export PKG_CONFIG_PATH=/home/user/tmp/usr/lib/pkgconfig

Now the library should be picked up by the filter's CMake process.


First Test
==========

To verify that your UFO version is behaving correctly, you should check its
functionality by running some builtin tests using ::

  $ make test

which is not that much of a help when things break and ::

  $ make gtest

which tells where the problems are. It also outputs a summary in
``core/tests/results.html``.
