class CallableFilter(object):
    def __init__(self, gobject, graph):
        self.gobject = gobject
        self.graph = graph
        self.name = gobject.get_plugin_name()

    def __call__(self, *args, **kwargs):
        n_inputs = self.gobject.get_num_inputs()

        if n_inputs != len(args):
            raise TypeError("`%s' receives only %i argument(s) but was called with %i parameters" % (self.name, n_inputs, len(args)))

        for i, target in enumerate(args):
            self.graph.connect_filters_full(target.gobject, 0, self.gobject, i, Ufo.TransferMode.DISTRIBUTE)

        return self


class NodeFactory(object):
    def __init__(self, plugin_manager, graph):
        self.pm = plugin_manager
        self.graph = graph

    def get(self, name, **kwargs):
        gobject = self.pm.get_filter(name)
        gobject.set_properties(kwargs)
        return CallableFilter(gobject, graph)
