from flask import Flask, render_template, jsonify, request
from gi.repository import GObject, Ufo

app = Flask(__name__)
graph = Ufo.Graph()
root = graph.get_root()

@app.route('/add_filter')
def add_filter():
    filter_name = request.args.get('filter')
    plugin = graph.get_filter(str(filter_name))
    props = [ "\"%s\" : \"%s\", " % (prop.name, plugin.get_property(prop.name)) for prop in GObject.list_properties(plugin) ]
    props = ''.join(props)
    props = "{" + props[:-2] + "}"
    root.add_element(plugin)
    return props


@app.route('/')
def index():
    filters = [ {'href':'add_filter/%s' % name, 'caption':name } for name in graph.get_filter_names()]
    return render_template('main.html', filters=filters)

if __name__ == '__main__':
    app.run()
