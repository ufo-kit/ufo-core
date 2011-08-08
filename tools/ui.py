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
            # FIXME: better use a wrapper around Sequence
            if isinstance(child, Ufo.Sequence):
                self.draw_sequence(cr, child, x, y)
                return

            cr.set_source_rgb(0.1, 0.1, 0.1)
            cr.move_to(x, y)
            name = child.get_plugin_name()
            cr.show_text(name)

            width, height, x_advance, y_advance = cr.text_extents(name)[2:]
            ve = None
            try:
                ve = self.elements[child]
            except KeyError:
                ve = VisualElement(child)
                self.elements[child] = ve

            ve.position = (x, y-height)
            ve.extents = (width, height)

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
        self.element_selection = self.builder.get_object("element_treeview").get_selection()

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
        scrolled_window.add_with_viewport(self.board)

    def __show_filter_properties(self, filter_object):
        scroll_container = self.builder.get_object("scrolled_properties")
        vbox = scroll_container.get_child()
        if vbox is not None:
            scroll_container.remove(vbox)
            vbox.unref()
        vbox = Gtk.VBox()
        scroll_container.add_with_viewport(vbox)
        for prop in GObject.list_properties(filter_object):
            # generate widget according to type
            type_name = prop.value_type.name
            value_widget = None
            value = filter_object.get_property(prop.name)
            if type_name == 'gchararray':
                value_widget = Gtk.Entry()
                value_widget.set_text(str(value))
                value_widget.connect('delete-text', self.on_property_delete_text, (filter_object, prop.name))
                value_widget.connect('insert-text', self.on_property_insert_text, (filter_object, prop.name))
            elif type_name == 'gint':
                value_widget = Gtk.SpinButton()
                value_widget.set_increments(1, 5)
                value_widget.set_range(prop.minimum, prop.maximum)
                value_widget.set_value(value)
                value_widget.connect('value-changed', self.on_property_int_change, (filter_object, prop.name))
            elif type_name == 'gdouble':
                value_widget = Gtk.SpinButton()
                value_widget.set_increments(0.01, 0.5)
                value_widget.set_digits(5)
                value_widget.set_range(prop.minimum, prop.maximum)
                value_widget.set_value(value)
                value_widget.connect('value-changed', self.on_property_float_change, (filter_object, prop.name))
            elif type_name == 'gboolean':
                value_widget = Gtk.CheckButton()
                value_widget.set_active(value)
                value_widget.connect('toggled', self.on_property_boolean_change, (filter_object, prop.name))

            value_widget.show()
            hbox = Gtk.HBox()
            label = Gtk.Label()
            label.set_label(prop.name + ":")
            label.show()
            hbox.pack_start_defaults(label)
            hbox.pack_end_defaults(value_widget)
            hbox.show()
            vbox.pack_end_defaults(hbox)
        vbox.show()

    def __connect_new_element(self, element):
        visual_filter = VisualElement(element)
        visual_filter.connect('clicked', self.on_element_clicked)
        self.board.add_element(visual_filter)

    def __connect_elements(self, container):
        for child in container.get_elements():
            if isinstance(child, Ufo.Sequence):
                self.__connect_elements(child)
                return
            self.__connect_new_element(child)

    def on_open(self, widget, user_data=None):
        dialog = Gtk.FileChooserDialog(Gtk.FileChooserAction.OPEN)
        dialog.add_buttons(Gtk.STOCK_OK, Gtk.ResponseType.OK, Gtk.STOCK_CLOSE, Gtk.ResponseType.CLOSE)
        file_filter = Gtk.FileFilter()
        file_filter.set_name("JSON Configuration (*.json)")
        file_filter.add_pattern('*.json')
        dialog.add_filter(file_filter)
        if dialog.run() == Gtk.ResponseType.OK:
            self.graph.read_from_json(dialog.get_filename())
            self.__connect_elements(self.board.root)
        dialog.destroy()

    def on_property_delete_text(self, widget, start_pos, end_pos, user_data):
        filter_object, prop_name = user_data
        filter_object.set_property(prop_name, widget.get_text()[:-1])

    def on_property_insert_text(self, widget, new_text, length, position, user_data):
        filter_object, prop_name = user_data
        filter_object.set_property(prop_name, widget.get_text() + new_text)

    def on_property_int_change(self, widget, user_data):
        filter_object, prop_name = user_data
        filter_object.set_property(prop_name, int(widget.get_value()))

    def on_property_float_change(self, widget, user_data):
        filter_object, prop_name = user_data
        filter_object.set_property(prop_name, widget.get_value())

    def on_property_boolean_change(self, widget, user_data):
        filter_object, prop_name = user_data
        filter_object.set_property(prop_name, widget.get_active())

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
            self.__connect_new_element(filter_object)
            self.board.selected_element = filter_object
            self.board.queue_draw()

    def on_element_clicked(self, visual_element):
        filter_object = visual_element.element
        self.__show_filter_properties(filter_object)

    def run(self, *args):
        self.builder.get_object("window1").show()
        Gtk.main()

    def quit(self, *args):
        Gtk.main_quit()

if __name__ == '__main__':
    Application().run()
