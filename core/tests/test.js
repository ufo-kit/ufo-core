const Ufo = imports.gi.Ufo

let graph = new Ufo.Graph()
graph.read_from_json("test.json")
graph.run()
