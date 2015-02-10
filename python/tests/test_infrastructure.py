import tifffile
from gi.repository import GLib, Ufo
from nose.tools import raises
from common import tempdir, disable


@raises(GLib.GError)
def test_wrong_connection():
    from ufo import Reader, FlatFieldCorrection, Writer

    dark_reader = Reader()
    flat_reader = Reader()
    radio_reader = Reader()
    writer = Writer()
    ffc = FlatFieldCorrection()

    g = Ufo.TaskGraph()
    g.connect_nodes_full(radio_reader.task, ffc.task, 0)
    g.connect_nodes_full(dark_reader.task, ffc.task, 1)
    g.connect_nodes_full(flat_reader.task, ffc.task, 0)
    g.connect_nodes(ffc.task, writer.task)

    sched = Ufo.Scheduler()
    sched.run(g)


@disable
def test_task_count():
    from ufo import Generate, Writer, Averager

    with tempdir() as d:
        generate = Generate(number=5, width=512, height=512)
        writer = Writer(filename=d.path('foo-%i.tif'))
        average = Averager()

        writer(average(generate())).run().join()

        assert(generate.task.props.num_processed == 0)
        assert(average.task.props.num_processed == 5)
        assert(writer.task.props.num_processed == 1)


@disable
def test_broadcast():
    from ufo import Generate, Writer
    import glob

    with tempdir() as d:
        generate = Generate(number=5, width=512, height=512)
        write1 = Writer(filename=d.path('foo-%i.tif'))
        write2 = Writer(filename=d.path('bar-%i.tif'))

        g = Ufo.TaskGraph()
        g.connect_nodes(generate.task, write1.task)
        g.connect_nodes(generate.task, write2.task)

        sched = Ufo.Scheduler()
        sched.run(g)

        foos = glob.glob(d.path('foo-*'))
        bars = glob.glob(d.path('bar-*'))
        assert(len(foos) == 5)
        assert(len(bars) == 5)
