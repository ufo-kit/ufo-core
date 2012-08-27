.. _using-objects:

====================
Technical Background
====================

Configuration
=============

There are two different notions of configuration in the Ufo framework: per-node
configuration and execution configuration. The former is realized with GObject
properties. In Python, these properties can be set as a named parameter with
``set_properties`` or assigned to the property as part of the ``props``::

    writer = pm.get_filter('reader')

    writer.props.prefix = 'foo'
    writer.set_properties(prefix='foo')

The execution configuration is independent of the parameters of the actual
computation and used to determine environment specific foos. For example, if the
filters are not installed system wide, there need to be a way to tell the
framework were these are located. This information is stored in an
``Ufo.Configuration`` object. Each part of the framework that implements the
``Ufo.Configurable`` interface accepts such an object at construction time and
uses necessary information stored within::

    # Lets assume that filters and .cl files are stored in the parent directory.
    # So we create a new configuration object and set its `paths' property.
    config = Ufo.Configuration(paths=['..'])

    # The PluginManager is configurable ...
    pm = Ufo.PluginManager(configuration=config)

    # ... so is the scheduler
    scheduler = Ufo.Scheduler(configuration=config)


Profiling
=========
