.. _installation-linux:

Installation on Linux
=====================

Debian and RPM packages are provided. To install the Debian package under Ubuntu
or Debian, issue ::

  $ sudo dpkg -i libufo-x.y.deb

Unfortunately, the OpenCL distributions from NVIDIA and AMD are not known to the
repository system of openSUSE. When installing UFO, ``rpm`` will complain, that
it cannot find this dependency. To solve this problem use ``zypper`` ::

  $ sudo zypper install libufo-x.y.rpm

and when asked to fix the dependency, ignore it.


Building UFO from Source
========================

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
---------------------

OpenCL development files must be installed in order to build UFO. However, we
cannot give general advices as installation procedures vary between different
vendors. However, our CMake build facility is in most cases intelligent enough
to find header files and libraries.


Checking out the Code
---------------------

In an empty directory, issue the following commands to retrieve the current HEAD
of the source ::

  $ bzr clone bzr+ssh://<user>@ufo.kit.edu/vogelgesang/ufo


Configuration and Compilation
-----------------------------

Change into another empty `build` directory and issue the following commands to
configure ::

  $ cmake <path-to-ufo>

CMake will notify you, if some of the dependencies are not met. Remember though,
that only GLib/GObject, JSON-GLib and OpenCL are needed. Please consult your
distributions documentation to install the necessary development libraries.  If
other dependencies are not satisified, the relevant filter plugins will not be
built.

You can adjust some build parameters later on by using the ``ccmake`` tool in
the build directory ::

  $ ccmake .

For early versions of PyGObject, it is necessary that the introspection files
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
  $ ldconfig

First Test
----------

To verify that your UFO version is behaving correctly, you should check its
functionality by running some builtin tests using ::

  $ make test

which is not that much of a help when things break and ::

  $ make gtest

which tells where the problems are. It also outputs a summary in
``core/tests/results.html``.
