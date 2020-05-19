
import math
import numpy as np


class Parameters(object):

    def __init__(self, data):
        self.num_sinograms = data.shape[0]
        self.num_projections = data.shape[1]
        self.width = data.shape[2]
        self.data = data

    def __repr__(self):
        return 'Parameters(n_sinos={}, n_proj={}, width={})'.format(self.num_sinograms, self.num_projections, self.width)

    @property
    def sinograms(self):
        return (self.data[i,:,:] for i in range(self.num_sinograms))


def _fbp(data, center, volume, theta, kwargs):
    from ufo import Fft, Ifft, Filter, Backproject

    p = Parameters(data)
    theta = theta[1] - theta[0]
    center = np.mean(center)

    bp = Backproject(axis_pos=center, angle_step=theta, angle_offset=np.pi)
    fft = Fft()
    ifft = Ifft(crop_width=p.width)
    flt = Filter()

    for i, slce in enumerate(bp(ifft(flt(fft(p.sinograms))))):
        volume[i,:,:] = slce


def fbp(*args, **kwargs):
    """
    Reconstruct object using UFO's filtered backprojection algorithm

    Note
    ----
    You have to set ``ncore`` to 1 because UFO provides multi-threading on its
    own.
    """

    # Erm ... okay?
    result = [_fbp]
    result.extend(args)
    result.append(kwargs)
    return result


def _dfi(data, center, volume, theta, kwargs):
    from ufo import Crop, Zeropad, Fft, Ifft, DfiSinc, Ifft, SwapQuadrants

    oversampling = kwargs['options'].get('oversampling', 1)
    p = Parameters(data)
    theta = theta[1] - theta[0]
    center = np.mean(center)
    padded_size = pow(2, int(math.ceil(math.log(p.width, 2))))
    frm = padded_size / 2 - p.width / 2
    to = padded_size / 2 + p.width / 2

    pad = Zeropad(oversampling=1, center_of_rotation=center)
    fft = Fft(dimensions=1, auto_zeropadding=False)
    dfi = DfiSinc(angle_step=theta)
    ifft = Ifft(dimensions=2)
    swap_forward = SwapQuadrants()
    swap_backward = SwapQuadrants()

    for i, slce in enumerate(swap_backward(ifft(swap_forward(dfi(fft(pad(p.sinograms))))))):
        volume[i,:,:] = slce[frm:to, frm:to]


def dfi(*args, **kwargs):
    """
    Reconstruct object using UFO's direct Fourier inversion algorithm

    Extra options
    -------------
    oversampling : int, optional
       Oversampling factor.

    Note
    ----
    You have to set ``ncore`` to 1 because UFO provides multi-threading on its
    own.
    """
    result = [_dfi]
    result.extend(args)
    result.append(kwargs)
    return result


def _ir(data, center, volume, theta, kwargs):
    from ufo import Null, Task
    p = Parameters(data)
    theta = theta[1] - theta[0]
    center = np.mean(center)

    null = Null()
    env = null.env


    allowed_methods = ('sart', 'sirt', 'asdpocs', 'sbtv')

    opts = kwargs['options']
    method_name = opts.get('method', 'sart')
    method_name = method_name if method_name in allowed_methods else 'sart'

    projector = env.pm.get_task_from_package('ir', 'parallel-projector')
    projector.set_properties(model='joseph', is_forward=False, axis_position=center, step=theta)

    method = env.pm.get_task_from_package('ir', method_name)
    method.set_properties(projector=projector, num_iterations=opts.get('num_iterations', 5))

    if method_name in ('sart', 'sirt'):
        method.set_properties(relaxation_factor=opts.get('relaxation_factor', 0.25))

    if method_name == 'asdpocs':
        minimizer = env.pm.get_task_from_package('ir', 'sirt')
        method.set_properties(df_minimizer=minimizer)

    if method_name == 'sbtv':
        method.set_properties(mu=opts.get('mu', 0.5))

    method = Task(env, 'ir', {}, method)

    for i, slce in enumerate(method(p.sinograms)):
        volume[i,:,:] = slce[:,:]


def ir(*args, **kwargs):
    """
    Reconstruct object using UFO's direct iterative reconstruction methods

    Extra options
    -------------
    method : str
        Any of `sart`, `sirt`, `asdpocs` and `sbtv`.

    num_iterations : int, optional
        Number of iterations before stopping reconstruction.

    relaxation_factor : float
        Relaxation factor for SART and SIRT methods.

    mu : float
        mu for Split Bregman Total Variation algorithm.

    Note
    ----
    You have to set ``ncore`` to 1 because UFO provides multi-threading on its
    own.
    """
    result = [_ir]
    result.extend(args)
    result.append(kwargs)
    return result
