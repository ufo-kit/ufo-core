import pygtk
pygtk.require('2.0')
from gi.repository import GObject, Gtk, Gdk, Ufo, cairo
import cairo


class Board(Gtk.DrawingArea, GObject.GObject):
    """Visualizes the current configuration and corresponds to the View of the
    MVC pattern"""

    def __init__(self, graph):
        Gtk.DrawingArea.__init__(self)
        GObject.GObject.__init__(self)
        self.set_events(self.get_events() | Gdk.EventMask.EXPOSURE_MASK)
        self.connect('expose-event', self.do_expose_event)
        self.graph = graph
        self.root = graph.get_root()
        self.currently_selected = self.root
    
    def do_expose_event(self, area, event):
        area = event.expose.area
        cr = self.window.cairo_create()
        cr.rectangle(area.x, area.y, area.width, area.height)
        cr.clip()
        self.draw(cr, *self.window.get_size())

    def draw(self, cr, width, height):
        cr.set_source_rgb(1.0, 1.0, 1.0)
        cr.rectangle(0, 0, width, height)
        cr.fill()

        self.draw_sequence(cr, self.root, 30, 30)

    def draw_sequence(self, cr, sequence, x, y):
        for child in sequence.get_elements():
            print child.get_plugin_name()

    @property
    def selected_container(self):
        return self.currently_selected

    @selected_container.setter
    def selected_container(self, element):
        self.currently_selected = element


class Application(object):

    def __init__(self):
        self.builder = Gtk.Builder()
        self.builder.add_from_file("ui.xml")
        self.builder.connect_signals(self)
        self.element_store = self.builder.get_object("element_store")
        self.element_selection = self.builder.get_object("element_treeview").get_selection()
        self.graph = Ufo.Graph()

        self.__fill_elements()
        self.__create_canvas()

    def __fill_elements(self):
        self.filter_names = self.graph.get_filter_names()

        # insert the top-level elements
        self.container_element = self.element_store.insert(None, 0)
        self.filter_element = self.element_store.insert(None, 1)
        self.element_store.set_value(self.container_element, 1, "Containers")
        self.element_store.set_value(self.filter_element, 1, "Filters")

        # add filters
        for filter_name in self.filter_names:
            child = self.element_store.prepend(self.filter_element)
            self.element_store.set_value(child, 1, filter_name)

    def __create_canvas(self):
        self.board = Board(self.graph)
        self.board.show()
        hpane = self.builder.get_object("hpane")
        scrolled_window = hpane.get_child1()
        scrolled_window.add(self.board)

    def on_button_add_element_clicked(self, data):
        rows = self.element_selection.get_selected_rows()[0]
        if not rows:
            return

        tree_path = rows[0]
        tree_iter = self.element_store.get_iter(tree_path)[1]
        name = self.element_store.get_value(tree_iter, 1)
        if name not in ["Containers", "Filters"]:
            filter_object = self.graph.get_filter(name)
            self.board.selected_container.add_element(filter_object)
            self.board.queue_draw()

    def run(self, *args):
        self.builder.get_object("window1").show()
        Gtk.main()

    def quit(self, *args):
        Gtk.main_quit()

if __name__ == '__main__':
    Application().run()
