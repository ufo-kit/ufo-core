import numpy as np
import tifffile
from common import tempdir, disable
from gi.repository import Ufo


def test_core_issue_61_16_bit_tiffs():
    from ufo import Reader, Writer

    orig = np.random.randint(0, 65535, (512, 512)).astype(np.uint16)

    with tempdir() as d:
        tifffile.imsave(d.path('temp.tif'), orig)

        read = Reader(path=d.path('temp.tif'))
        write = Writer(filename=d.path('temp-%i.tif'))

        write(read()).run().join()

        produced = tifffile.imread(d.path('temp-0.tif'))
        assert(np.sum(orig - produced) == 0.0)


@disable
def test_core_issue_64_fixed_expansion():
    g = Ufo.TaskGraph()
    pm = Ufo.PluginManager()
    sched = Ufo.FixedScheduler()
    arch = Ufo.ArchGraph()
    gpus = arch.get_gpu_nodes()
    sched.set_gpu_nodes(arch, gpus)

    generate = pm.get_task('generate')
    null = pm.get_task('null')

    generate.set_properties(number=5, width=512, height=512)

    for gpu in gpus:
        median = pm.get_task('median-filter')
        median.set_proc_node(gpu)
        g.connect_nodes(generate, median)
        g.connect_nodes(median, null)

    sched.run(g)


@disable
def test_core_issue_66_remote_list():
    daemon = Ufo.Daemon.new('tcp://127.0.0.1:5554')
    daemon.start()
    arch = Ufo.ArchGraph(remotes=['tcp://127.0.0.1:5554'])
    daemon.stop()
