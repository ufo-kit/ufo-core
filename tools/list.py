import pygtk
pygtk.require('2.0')

from gi.repository import GObject, Ufo

if __name__ == '__main__':
    graph = Ufo.Graph()
    filters = graph.get_filter_names()
    for f in sorted(filters):
        print 'Filter "%s"' % f
        plugin = graph.get_filter(f)
        props = GObject.list_properties(plugin)

        for prop in props:
            print '  "%s": %s' % (prop.name, prop.blurb)

        print
