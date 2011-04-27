.. _building:

Building UFO from Source
========================

UFO has only a few hard source dependencies, namely

  - `GLib 2.0 <http://developer.gnome.org/glib/stable/>`_ and
  - a valid OpenCL installation.

Furthermore it is convenient to build the framework and accompanying
documentation with a recent version of `CMake <http://cmake.org>`_, `Doxygen
<http://doxygen.org>`_ and `Sphinx <http://sphinx.pocoo.org>`_. The current
distributed version control system in use, is `Bazaar <bazaar.canonical.com>`_.

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

CMake will notify you, if some of the dependencies are not met. Please consult
your distributions documentation to install the necessary development libraries
You can adjust some build parameters later on by using the ``ccmake`` tool in
the build directory::

  $ ccmake .

Last but not least build the framework using ::

  $ make

But hold on, there are even more ``make`` targets. You may issue ::

  $ make doxygen    

to create a plain old Doxygen documentation of the API and afterwards create
`this` documentation by typing ::

  $ make docs

If you have a fairly recent CMake version, this command will pull in the `Breathe`
Doxygen-to-Sphinx bridge via Git and include API references in the final output.

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
