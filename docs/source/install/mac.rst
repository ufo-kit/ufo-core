.. _installation-mac:

Installation on MacOS X (Lion 10.7)
===================================

Preface: This information is kindly provided by Andrey Shkarin and Roman
Shkarin.

.. highlight:: bash

1. Install the MacPorts from http://macports.org

   .. note:: 
   
       If you previously installed MacPorts, and it can not be started after
       latest installation. `Error: port dlopen (...`
       You must download the tar.gz file and install it using a terminal::

           ./configure
           make
           sudo make install

2. Clone Ethos from the repository::

       git clone git://git.dronelabs.com/ethos

3. Install the necessary packages through macports::

       sudo port install glib2
       sudo port install gtk2
       sudo port install json-glib

4. Install CMake from http://cmake.org

.. highlight:: c

5. Next, go to the directory of Ethos and edit the file ``ethos_dir/ethos/ethos_manager.c`` (from line 194) from::

       while (NULL != (filename = g_dir_read_name (dir))) {
           if (g_str_has_suffix (filename, "." G_MODULE_SUFFIX)) {
               abspath = g_build_filename (loaders_dir, filename, NULL);
               loader = ethos_manager_create_plugin_loader (manager, abspath);
               if (loader != NULL)
                    loaders = g_list_prepend (loaders, loader);}}
               g_free (abspath);
           }
       }

   to::

       while (NULL != (filename = g_dir_read_name (dir))) {
           #ifdef __APPLE__
               gchar* suffix = "dylib";
           #else
               gchar* suffix = ("." G_MODULE_SUFFIX);
           #endif
            if (g_str_has_suffix (filename, "." G_MODULE_SUFFIX)) {
                abspath = g_build_filename (loaders_dir, filename, NULL);
                loader = ethos_manager_create_plugin_loader (manager, abspath);
                if (loader != NULL)
                     loaders = g_list_prepend (loaders, loader);}}
                g_free (abspath);
            }
        }

   This fixes a bug where the application could not find an
   ethos-plugin-loader.

.. highlight:: bash

6. Make and install Ethos

   1. Disable making for: ``ethos-dir/tests``, change in ``Makefile.am`` from (line
      number: contents)::

       02: SUBDIRS = c-plugins manager-dep
       30: manager_sources = manager.c
       31: plugin_info_sources = plugin-info.c

   2. Make and Install Ethos::

       sh autogen.sh
       make
       sudo make install

7. Make ufo-core

   1. Got to the directory ufo-core and run::

       export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
       cmake .

   2. Run::

       make
       sudo make install

   3. Installation is complete, perhaps the last lines are as follows::

       -- Installing: /usr/local/lib/pkgconfig/ufo.pc
       CMake Error at src/bindings/cmake_install.cmake:33 (FILE):
       file INSTALL cannot find
         "/Users/Andrey/Desktop/ufo-distr/ufo-core/src/bindings/../src/Ufo-0.1.gir".
       Call Stack (most recent call first):
         src/cmake_install.cmake:60 (INCLUDE)
         cmake_install.cmake:32 (INCLUDE)
       make: *** [install] Error 1

8. Make filters

   1. Go to ufo-filters directory. Now, since libufo was installed in lib64, we must update the paths to look
   for shared libraries::

       export DYLD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64
       export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

   2. Run::

       cmake .
       make
       sudo make install

9. Build the test project and verify that everything works.

