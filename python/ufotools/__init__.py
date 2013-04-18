import types
from gi.repository import Ufo


def _run(graph, config=None):
    sched = Ufo.Scheduler(config=config)
    sched.run(graph)


class Factory(object):
    """Wrapped task node factory."""
    def __init__(self, config=None):
        self.pm = Ufo.PluginManager(config=config)
        self.graph = Ufo.TaskGraph()
        self.config = config

    def get(self, name, **kwargs):
        """Return a wrapped task instantiated from *name* and pre-populated
        with property values in the *kwargs* dict.

        Using

            read = factory.get('reader', path='/some/where')

        is equivalent to

            read = pluginmanager.get_task('reader')
            read.set_properties(path='/some/where')

        Any *wrapped* task can be call chained, which in turn is an implicit
        connection in the task graph. So, using

            write(avg(read))
            factory.run()

        is equivalent to

            g = Ufo.TaskGraph()
            g.connect_nodes(read, avg)
            g.connect_nodes(avg, write)

            s = Ufo.Scheduler()
            s.run(g)

        Note, that wrapped tasks also have a run() method, so you can actually
        start the computation with

            write(avg(read)).run()
        """
        task = self.pm.get_task(name)
        task.set_properties(**kwargs)

        task.__class__.__call__ = types.MethodType(self._connect, task)
        task.run = self.run

        return task

    def run(self):
        """Execute the wrapped graph."""
        _run(self.graph, self.config)

    def _connect(self, task, *args, **kwargs):
        for i, target in enumerate(args):
            self.graph.connect_nodes_full(target, task, i)

        return task
