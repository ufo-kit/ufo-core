from __future__ import absolute_import

import sys
import re
import time
import threading
import Queue as queue
import numpy as np
from gi.repository import Ufo
from .numpy import asarray, fromarray, fromarray_inplace


Ufo.Scheduler()


class Environment(object):

    def __init__(self, pm):
        self.pm = pm
        self.reset()
        self.done = False
        self.started = False

    def reset(self):
        self.graph = Ufo.TaskGraph()


class Task(object):

    def __init__(self, env, name, cargs):
        self.env = env
        self.name = name
        self.cargs = cargs
        self.task = self.env.pm.get_task(self.name)

    def connect(self, *args, **kwargs):
        self.task.connect(*args, **kwargs)

    def __call__(self, *args):
        self.task.set_properties(**self.cargs)

        if len(args) == 0:
            return self

        if any(isinstance(a, Task) for a in args):
            for i, source in enumerate(args):
                self.env.graph.connect_nodes_full(source.task, self.task, i)
        else:
            def input_data(*iargs):
                tasks = []
                buffers = []

                for i in range(len(iargs)):
                    task = Ufo.InputTask()
                    tasks.append(task)
                    buffers.append(None)
                    self.env.graph.connect_nodes_full(task, self.task, i)

                while not self.env.started:
                    time.sleep(0.01)

                for args in zip(*iargs):
                    for i, (task, data) in enumerate(zip(tasks, args)):
                        if buffers[i] is None:
                            buffers[i] = fromarray(data.astype(np.float32))
                        else:
                            buffers[i] = task.get_input_buffer()
                            fromarray_inplace(buffers[i], data.astype(np.float32))

                        task.release_input_buffer(buffers[i])

                for task in tasks:
                    task.stop()

            thread = threading.Thread(target=input_data, args=args)
            thread.start()

        return self

    def __iter__(self):
        return self.items()

    def run(self):
        sched = Ufo.FixedScheduler()

        def run_scheduler():
            self.env.done = False
            sched.run(self.env.graph)
            self.env.done = True
            self.env.reset()

        self.thread = threading.Thread(target=run_scheduler)
        self.thread.start()
        self.env.started = True
        return self

    def join(self):
        self.thread.join()

    def items(self):
        results = queue.Queue()

        def processed(task):
            buf = output_task.get_output_buffer()
            results.put(asarray(buf).copy())
            output_task.release_output_buffer(buf)

        output_task = Ufo.OutputTask()
        output_task.connect('processed', processed)
        self.env.graph.connect_nodes(self.task, output_task)
        self.run()

        while not self.env.done or not results.empty():
            try:
                yield results.get(True, 0.01)
            except queue.Empty:
                pass

        self.join()

    @property
    def graph(self):
        return self.env.graph


class TaskLoader(object):

    def __init__(self, env, name):
        self.env = env
        self.name = name

    def __call__(self, **kwargs):
        # This should probably be done with using metaclasses, but it looks
        # simple and straightforward enough
        return Task(self.env, self.name, kwargs)


def get_path(fqn):
    return fqn.split('.')[1]


class Importer(object):
    def __init__(self):
        self.env = Environment(Ufo.PluginManager())
        self.task_names = self.env.pm.get_all_task_names()

    def find_module(self, fqn, path=None):
        if fqn.startswith('ufo.'):
            if get_path(fqn).lower() in self.task_names:
                return self

        return None

    def load_module(self, fqn):
        # Splits according to http://stackoverflow.com/a/1176023/997768
        s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', get_path(fqn))
        task_name = re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()
        return TaskLoader(self.env, task_name)


sys.meta_path = [Importer()]
