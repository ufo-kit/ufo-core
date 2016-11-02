from __future__ import absolute_import
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
    dfi = DfiSinc()
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
