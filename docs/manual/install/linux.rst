.. _installation-linux:

#####################
Installation on Linux
#####################

====================
Building from source
====================

UFO has only a few hard source dependencies, namely

  - `GLib 2.0 <http://developer.gnome.org/glib/stable/>`_,
  - `JSON-GLib 1.0 <http://live.gnome.org/JsonGlib>`_
  - `ZeroMQ 3.2 <http://zeromq.org>`_ and
  - a valid OpenCL installation.

Furthermore, it is necessary to build the framework with a recent version of
`CMake <http://cmake.org>`_.  `Sphinx <http://sphinx.pocoo.org>`_ is used to
create this documentation and `Bazaar <bazaar.canonical.com>`_ for revision
control.

In case you use openSUSE, just issue ::

    $ zypper install gcc gcc-c++ glib2-devel json-glib-devel
    $ zypper install gobject-introspection-devel python-gobject2
    $ zypper install gtk-doc python-Sphinx
    $ zypper install libtiff-devel

to install dependencies from the package repositories. Depending on which system
you are using you have to install ZeroMQ 3.2 from source ::

    $ wget http://download.zeromq.org/zeromq-3.2.2.tar.gz
    $ tar xfz zeromq-3.2.2.tar.gz
    $ cd zeromq-3.2.2
    $ ./configure && make && make distcheck
    $ make install

OpenCL development files must be installed in order to build UFO. However, we
cannot give general advices as installation procedures vary between different
vendors. However, our CMake build facility is in most cases intelligent enough
to find header files and libraries for NVIDIA CUDA and AMD APP SDKs.


Retrieving the source code
==========================

In an empty directory, issue the following commands to retrieve the current
unstable version of the source::

    $ git clone https://github.com/ufo-kit/ufo-core
    $ git clone https://github.com/ufo-kit/ufo-filters
    $ git clone https://github.com/ufo-kit/oclfft

The latter is used for developers who have write-access to the corresponding
repositories. All stable versions are tagged. To see a list of all releases
issue::

    $ git tag -l


Quick deployment
================

UFO comes with a deploy script located in ``$UFO_ROOT/tools``. If executed
without any arguments it will try to build and install ZeroMQ, UFO (core and
filters) and oclfft for ``/usr/local``. You can set the installation prefix as
an argument to install it into your home directory.


System-wide installation
========================

If you have root access on the build machine, you can install the libraries and
tools system-wide so that every user can access them.

Building ufo-core
-----------------

Change into another empty `build` directory and issue the following commands to
configure ::

  $ cmake <path-to-ufo>

CMake will notify you, if some of the dependencies are not met. In case you want
to install the library system-wide on a 64-bit machine you should generate the
Makefiles with ::

  $ cmake <path-to-ufo> -DLIBDIR=/usr/lib64

For earlier versions of PyGObject, it is necessary that the introspection files
are located under ``/usr`` not ``/usr/local``. You can force the prefix by
calling ::

  $ cmake <path-to-ufo> -DPREFIX=/usr

Last but not least build the framework, introspection files, API reference and
the documentation using ::

  $ make

You should now run some basic tests with ::

  $ make test

If everything went well, you can install the library with ::

  $ make install

You can also build ``RPM`` and ``DEB`` packages with ::

  $ make package

and source tarballs with ::

  $ make package_source

.. seealso:: :ref:`faq-linker-cant-find-libufo`


Building ufo-filters
--------------------

Once ufo-core is installed you can build the filter suite in a pretty similar
way ::

    $ mkdir -p build/ufo-filters
    $ cd build/ufo-filters
    $ cmake <path-to-ufo-filters> -DLIBDIR=/usr/lib64 -DPREFIX=/usr
    $ make
    $ make install


.. _inst-installing-into-non-standard-directories:

Installing into non-standard directories
========================================

It is possible to install the library in a non-standard directory, for example
in the home directory of a user. In case we want to install in ``~/tmp/usr``, we
have to configure ufo-core like this ::

  $ mkdir -p build/ufo-core
  $ cd build/ufo-core
  $ cmake <path-to-ufo> -DPREFIX=/home/user/tmp/usr
  $ make && make install

Now, we have to adjust the ``pkg-config`` path, so that the library can be
found when configuring the filters ::

  $ export PKG_CONFIG_PATH=/home/user/tmp/usr/lib/pkgconfig
  $ mkdir -p build/ufo-filters
  $ cd build/ufo-filters
  $ cmake <path-to-ufo-core> -DPREFIX=/home/user/tmp/usr
  $ make && make install

After installation you have to set the typelib and linker path so that
everything is found at run-time ::

  $ export GI_TYPELIB_PATH=/home/user/tmp/usr/lib/girepository-1.0
  $ export LD_LIBRARY_PATH=/home/user/tmp/usr/lib:$LD_LIBRARY_PATH

.. note::

    It is strongly discouraged to abuse the library path for permanent
    usage. Read some good arguments `here`__ and `here`__.

__ http://web.archive.org/web/20060719201954/http://www.visi.com/~barr/ldpath.html
__ http://linuxmafia.com/faq/Admin/ld-lib-path.html
