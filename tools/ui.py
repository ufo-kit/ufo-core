import pygtk
pygtk.require('2.0')
from gi.repository import GObject, Gtk, Gdk, Ufo, cairo


class VisualElement(GObject.GObject):
    """Wraps Ufo.Filter to make it clickable"""
    __gsignals__ = { 'clicked' : (GObject.SIGNAL_RUN_LAST, GObject.TYPE_NONE, ()), }

    def __init__(self, element):
        GObject.GObject.__init__(self)

        self.element = element
        self.position = None
        self.extents = None

    def handle_click(self, cx, cy):
        if not self.position or not self.extents:
            return

        x, y = self.position
        width, height = self.extents
        if cx >= x and cx <= x + width and cy >= y and cy <= y + width:
            self.emit('clicked')
            return True

        return False


class Board(Gtk.DrawingArea, GObject.GObject):
    """Visualizes the current configuration and corresponds to the View of the
    MVC pattern"""

    def __init__(self, graph):
        Gtk.DrawingArea.__init__(self)
        GObject.GObject.__init__(self)

        self.set_events(self.get_events() | 
                Gdk.EventMask.EXPOSURE_MASK |
                Gdk.EventMask.BUTTON_PRESS_MASK)
        self.connect('expose-event', self.do_expose_event)
        self.connect('button-press-event', self.on_button_press)

        self.graph = graph
        self.root = graph.get_root()
        self.current_container = self.root
        self.selected_element = None
        self.elements = {}

    def add_element(self, visual_element):
        self.current_container.add_element(visual_element.element)
        self.elements[visual_element.element] = visual_element

    def on_button_press(self, board, event):
        for element in self.elements.values():
            if element.handle_click(event.button.x, event.button.y):
                self.selected_element = element.element

        self.queue_draw()
    
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
        cr.select_font_face("Sans")
        cr.set_font_size(12);

        for child in sequence.get_elements():
            cr.set_source_rgb(0.1, 0.1, 0.1)
            cr.move_to(x, y)
            name = child.get_plugin_name()
            cr.show_text(name)

            width, height, x_advance, y_advance = cr.text_extents(name)[2:]
            try:
                visual_element = self.elements[child]
                visual_element.position = (x, y-height)
                visual_element.extents = (width, height)
            except KeyError:
                pass 

            if child == self.selected_element:
                cr.set_source_rgb(1.0, 0.1, 0.1)

            cr.rectangle(x, y-height, width, height)
            cr.stroke()
            x += x_advance
            y += y_advance


class Application(object):

    def __init__(self):
        self.builder = Gtk.Builder()
        self.builder.add_from_file("ui.xml")
        self.builder.connect_signals(self)
        self.element_store = self.builder.get_object("element_store")
        self.property_store = self.builder.get_object("property_store")
        self.element_selection = self.builder.get_object("element_treeview").get_selection()
        self.property_selection = self.builder.get_object("property_treeview").get_selection()
        value_cell = self.builder.get_object("value_cell")
        value_cell.set_property("editable", True)

        self.graph = Ufo.Graph()

        self.__fill_elements()
        self.__create_board()

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

    def __create_board(self):
        self.board = Board(self.graph)
        self.board.show()
        hpane = self.builder.get_object("hpane")
        scrolled_window = hpane.get_child1()
        scrolled_window.add(self.board)

    def __show_filter_properties(self, filter_object):
        self.property_store.clear()
        props = GObject.list_properties(filter_object)
        for prop in props:
            prop_value = str(filter_object.get_property(prop.name))
            row = self.property_store.append([prop.name, prop_value])
            self.property_store.set_value(row, 1, prop_value)

    def on_button_add_element_clicked(self, data):
        rows = self.element_selection.get_selected_rows()[0]
        if not rows:
            return

        tree_path = rows[0]
        tree_iter = self.element_store.get_iter(tree_path)[1]
        name = self.element_store.get_value(tree_iter, 1)
        if name not in ["Containers", "Filters"]:
            filter_object = self.graph.get_filter(name)
            self.__show_filter_properties(filter_object)

            self.board.selected_element = filter_object

            visual_filter = VisualElement(filter_object)
            visual_filter.connect('clicked', self.on_element_clicked)
            self.board.add_element(visual_filter)
            self.board.queue_draw()

    def on_element_clicked(self, visual_element):
        filter_object = visual_element.element
        self.__show_filter_properties(filter_object)

    def on_value_cell_edited(self, cell_renderer, position, new_value):
        rows = self.property_selection.get_selected_rows()[0]
        tree_path = rows[0]
        tree_iter = self.property_store.get_iter(tree_path)[1]
        prop_name = self.property_store.get_value(tree_iter, 0)
        # Update objects property
        self.board.selected_element.set_property(prop_name, new_value)
        self.__show_filter_properties(self.board.selected_element)

    def run(self, *args):
        self.builder.get_object("window1").show()
        Gtk.main()

    def quit(self, *args):
        Gtk.main_quit()

if __name__ == '__main__':
    Application().run()
