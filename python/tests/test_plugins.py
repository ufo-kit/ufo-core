import ufo.numpy
import numpy as np
import tifffile
import contextlib
from common import disable, tempdir


a, b = 1.5, 2.5
ones = np.ones((512, 512))
zeros = np.zeros((512, 512))
small = np.ones((256, 256))
random = np.random.random((512, 512))


def have_camera_plugin():
    from gi.repository import Ufo

    return 'camera' in Ufo.PluginManager().get_all_task_names()


@contextlib.contextmanager
def single_tiff_setup(n_images, fmt='foo-{:05}.tif'):
    with tempdir() as d:
        data = np.ones((512, 512), dtype=np.float32)

        for i in range(n_images):
            tifffile.imsave(d.path(fmt.format(i)), data)

        yield d


def test_read_single_tiffs():
    from ufo import Read, Null

    with single_tiff_setup(32) as d:
        read = Read(path=d.root)
        null = Null()

        null(read()).run().join()
        assert(null.task.props.num_processed == 32)


def test_read_single_tiffs_stepped():
    from ufo import Read, Null

    with single_tiff_setup(32) as d:
        read = Read(path=d.root, step=2)
        null = Null()
        null(read()).run().join()
        assert(null.task.props.num_processed == 32 / 2)


def test_read_single_tiffs_start_modified():
    from ufo import Read, Null

    with single_tiff_setup(32) as d:
        read = Read(path=d.root, start=15)
        null = Null()

        null(read()).run().join()
        assert(null.task.props.num_processed == 32 - 15)


@disable
def test_read_multi_tiffs():
    from ufo import Read, Null

    with tempdir() as d:
        n_images = 32
        data = np.ones((512, 512, n_images))
        tifffile.imsave(d.path('foo.tif'), data)

        read = Read(path=d.path('foo.tif'))
        null = Null()

        null(read()).run().join()
        assert(null.task.props.num_processed == n_images)


def test_average():
    from ufo import Average
    average = Average()

    for x in average([a * ones, b * ones]):
        expected = (a + b) / 2
        assert(np.all(x == expected))


def test_buffer():
    from ufo import DummyData, Buffer

    data = DummyData(number=10, width=512, height=256)
    buffr = Buffer()
    result = list(buffr(data()))
    assert(len(result) == 10)

    for r in result:
        assert(r.shape[0] == 256)
        assert(r.shape[1] == 512)


def test_rescale():
    from ufo import Rescale
    rescale = Rescale(factor=0.5)
    result = list(rescale([a * ones, b * small]))
    assert(np.mean(result[0]) == a)
    assert(np.mean(result[1]) == b)


@disable
def test_roi():
    from ufo import CutRoi

    x, y = 10, 20
    w, h = 256, 128
    roi = CutRoi(x=x, y=y, width=w, height=h)
    result = list(roi([random, random]))
    ref = random[y:y+h, x:x+w]
    assert(ref.shape[0] == h)
    assert(ref.shape[1] == w)
    assert(np.all(ref == result[0]))


def test_stack():
    from ufo import Stack
    stack = Stack(number=2)

    for x in stack([a * ones, b * ones]):
        assert(x.shape[0] == 2)
        assert(np.all(x[0,:,:] == a))
        assert(np.all(x[1,:,:] == b))


def test_flatten():
    from ufo import FlattenInplace

    summing = FlattenInplace(mode="sum")
    result = list(summing([a * ones, b * ones]).items())
    assert(np.all(result[0] == a + b))


def test_fft_1d():
    from ufo import Fft, Ifft

    fft = Fft(dimensions=1)
    ifft = Ifft(dimensions=1)
    orig_a = a * ones
    orig_b = b * random
    result = list(ifft(fft([orig_a, orig_b])))

    assert(np.sum(orig_a - result[0]) < 0.001)
    assert(np.sum(orig_b - result[1]) < 0.01)


def test_fft_2d():
    from ufo import Fft, Ifft

    fft = Fft(dimensions=2)
    ifft = Ifft(dimensions=2)
    orig_a = a * ones
    orig_b = b * random
    result = list(ifft(fft([orig_a, orig_b])))

    assert(np.sum(orig_a - result[0]) < 0.001)
    assert(np.sum(orig_b - result[1]) < 0.1)


def test_flatfield_correction():
    from ufo import FlatFieldCorrect

    darks = np.ones((512, 512)) * 1.5
    flats = np.ones((512, 512)) * 11.5
    projs = np.ones((512, 512)) * 100.0
    ffc = FlatFieldCorrect()

    expected = (projs - darks) / (flats - darks)
    result = list(ffc([projs, projs], [darks, darks], [flats, flats]))[0]
    assert(np.sum(np.abs(expected - result)) < 1)

    expected = - np.log((projs - darks) / (flats - darks))
    ffc = FlatFieldCorrect(absorption_correct=True)
    result = list(ffc([projs, projs], [darks, darks], [flats, flats]))[0]
    assert(np.sum(np.abs(expected - result)) < 1)


def test_measure():
    from ufo import Measure

    measures = []

    def measure_callback(m, a):
        measures.append(ufo.numpy.asarray(a))

    measure = Measure(metric='mean', axis=-1)
    measure.connect('result', measure_callback)
    measure([a * ones, b * ones]).run().join()
    assert(len(measures) == 2)


def test_dummy_data():
    from ufo import DummyData

    data = DummyData(number=10, width=256, height=128)
    result = list(data())
    assert(len(result) == 10)
    assert(all(r.shape[0] == 128 and r.shape[1] == 256 for r in result))


def test_metaballs():
    from ufo import Metaballs

    metaballs = Metaballs(number=5, number_balls=5, width=512, height=256)
    result = list(metaballs())
    assert(len(result) == 5)
    assert(all(r.shape[0] == 256 and r.shape[1] == 512 for r in result))


def test_transpose():
    from ufo import Transpose

    transpose = Transpose()
    ones = np.ones((256, 512))
    zeros = np.zeros((256, 128))
    result = list(transpose([ones, zeros]))
    assert(np.all(result[0] == ones.transpose()))
    assert(np.all(result[1] == zeros.transpose()))


def test_uca():
    if have_camera_plugin():
        from ufo import Camera

        camera = Camera(name='mock', number=2)
        result = list(camera())
        assert(len(result) == 2)


def test_uca_direct():
    try:
        from gi.repository import Ufo, Uca

        if have_camera_plugin():
            from ufo import Camera

            uca_pm = Uca.PluginManager()
            mock = uca_pm.get_camerav('mock', [])
            camera = Camera(camera=mock, count=3)

            result = list(camera())
            assert(len(result) == 3)
    except ImportError:
        pass
