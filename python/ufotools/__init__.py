from gi.repository import Ufo


class TaskWrapper(object):
    def __init__(self, task, graph):
        self.task = task
        self.graph = graph
        self.name = task.get_plugin_name()

    def __call__(self, *args, **kwargs):
        for i, target in enumerate(args):
            self.graph.connect_nodes_full(target.task, self.task, i)

        return self


class Factory(object):
    def __init__(self, config=None):
        self.pm = Ufo.PluginManager(config=config)
        self.graph = Ufo.TaskGraph()
        self.config = config

    def get(self, name, **kwargs):
        task = self.pm.get_task(name)
        task.set_properties(**kwargs)
        return TaskWrapper(task, self.graph)

    def run(self):
        sched = Ufo.Scheduler(config=self.config)
        sched.run(self.graph)
