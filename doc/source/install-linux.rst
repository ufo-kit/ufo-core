.. _installation-linux:

Installation on Linux
=====================

In the future, UFO can be installed painlessly from openSUSE and Debian packages.


Building UFO from Source
========================

UFO has only a few hard source dependencies, namely

  - `GLib 2.0 <http://developer.gnome.org/glib/stable/>`_, 
  - `Ethos 1.0 <http://git.dronelabs.com/ethos/about/>`_,
  - `JSON-GLib 1.0 <http://live.gnome.org/JsonGlib>`_ and
  - a valid OpenCL installation.

Furthermore it is convenient to build the framework and accompanying
documentation with a recent version of `CMake <http://cmake.org>`_, `Doxygen
<http://doxygen.org>`_ and `Sphinx <http://sphinx.pocoo.org>`_. The current
distributed version control system in use, is `Bazaar <bazaar.canonical.com>`_.


Building Dependencies
---------------------

If you want to use language bindings you need to install libethos with GObject
introspection support enabled::

  $ cd <path-to-ethos>
  $ ./configure --enable-introspection
  $ make
  $ make install
  
OpenCL development files must be installed in order to build UFO. However, we
cannot give general advices as installation procedures vary between different
vendors. However, our CMake build facility is in most cases intelligent enough
to find header files and libraries.


Checking out the Code
---------------------

In an empty directory, issue the following commands to retrieve the current HEAD
of the source::

  $ bzr clone bzr+ssh://<user>@ufo.kit.edu/vogelgesang/ufo


Configuration and Compilation
-----------------------------

Change into another empty `build` directory and issue the following commands to
configure::

  $ cmake <path-to-ufo>

CMake will notify you, if some of the dependencies are not met. Remember though,
that only GLib/GObject, JSON-GLib, Ethos and OpenCL are needed. Please consult
your distributions documentation to install the necessary development libraries.
If other dependencies are not satisified, the relevant filter plugins will not
be built.

You can adjust some build parameters later on by using the ``ccmake`` tool in
the build directory::

  $ ccmake .

Last but not least build the framework using ::

  $ make

But hold on, there are even more ``make`` targets. You may issue ::

  $ make doxygen    

to create plain old Doxygen documentation of the API and afterwards create
`this` documentation by typing ::

  $ make docs

If you have a fairly recent CMake version, this command will pull in the `Breathe`
Doxygen-to-Sphinx bridge via Git and include API references in the final output.
However, this target will only be available when Sphinx is installed.

You can then proceed to build installation packages in ``RPM`` and ``DEB``
format by issueing ::

  $ make packages


First Test
----------

To verify that your UFO version is behaving correctly, you should check its
functionality by running some builtin tests using ::

  $ make test

which is not that much of a help when things break and ::

  $ make gtest

which tells where the problems are. It also outputs a summary in
``core/tests/results.html``.
