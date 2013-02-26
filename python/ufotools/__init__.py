from gi.repository import Ufo

def _run(graph, config=None):
    sched = Ufo.Scheduler(config=config)
    sched.run(graph)

class TaskWrapper(object):
    def __init__(self, task, graph, config=None):
        self.task = task
        self.graph = graph
        self.name = task.get_plugin_name()
        self.config = config

    def __call__(self, *args, **kwargs):
        for i, target in enumerate(args):
            self.graph.connect_nodes_full(target.task, self.task, i)

        return self

    def run(self):
        _run(self.graph, self.config)


class Factory(object):
    def __init__(self, config=None):
        self.pm = Ufo.PluginManager(config=config)
        self.graph = Ufo.TaskGraph()
        self.config = config

    def get(self, name, **kwargs):
        task = self.pm.get_task(name)
        task.set_properties(**kwargs)
        return TaskWrapper(task, self.graph, self.config)

    def run(self):
        _run(self.graph, self.config)
