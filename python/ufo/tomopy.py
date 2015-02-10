from __future__ import absolute_import
import math
import tomopy
import numpy as np


def _set_data(dset, volume, overwrite):
    if overwrite:
        dset.data_recon = volume
    else:
        return volume


def _prepare(dset):
    data = dset.data
    n_proj, proj_height, proj_width = data.shape

    center = dset.center if hasattr(dset, 'center') else proj_width / 2
    theta = dset.theta[1] - dset.theta[0] if hasattr(dset, 'theta') else 180.0 / n_proj

    if np.max(dset.theta) > 90.0:
        theta = np.radians(theta)

    volume = np.empty((proj_height, proj_width, proj_width), dtype=np.float32)
    projections = (data[i,:,:] for i in range(n_proj))

    return center, theta, n_proj, proj_width, proj_height, projections, volume


def fbp(dset, overwrite=True):
    from ufo import Fft, Ifft, Filter, Backproject, SinoGenerator

    center, theta, n_proj, proj_width, _, projections, volume = _prepare(dset)

    sino = SinoGenerator(num_projections=n_proj)
    bp = Backproject(axis_pos=center, angle_step=theta)
    fft = Fft()
    ifft = Ifft(crop_width=proj_width)
    flt = Filter()

    for i, slce in enumerate(bp(ifft(flt(fft(sino(projections)))))):
        volume[i,:,:] = slce

    dset.logger.info("ufo-fb [ok]")
    return _set_data(dset, volume, overwrite)


def dfi(dset, oversampling=1, overwrite=True):
    from ufo import Zeropadding, Fft, Ifft, DfiSinc, Ifft, SwapQuadrants, SinoGenerator

    center, theta, n_proj, proj_width, _, projections, volume = _prepare(dset)
    padded_size = pow(2, int(math.log(proj_width, 2) + 0.5))
    frm = padded_size / 2 - proj_width / 2
    to = padded_size / 2 + proj_width / 2

    sino = SinoGenerator(num_projections=n_proj)
    pad = Zeropadding(oversampling=oversampling)
    fft = Fft(dimensions=1, auto_zeropadding=0)
    dfi = DfiSinc()
    ifft = Ifft(dimensions=2)
    swap_forward = SwapQuadrants()
    swap_backward = SwapQuadrants()

    for i, slce in enumerate(swap_backward(ifft(swap_forward(dfi(fft(pad(sino(projections)))))))):
        volume[i,:,:] = slce[frm:to, frm:to]

    dset.logger.info("ufo-dfi [ok]")
    return _set_data(dset, volume, overwrite)


try:
    def sart(dset, overwrite=True):
        from ufo import Null, Ir, SinoGenerator, Buffer

        center, theta, n_proj, proj_width, _, projections, volume = _prepare(dset)

        # FIXME: stupid hack
        sinogen = SinoGenerator(num_projections=n_proj)
        env = sinogen.env

        geometry = env.pm.get_plugin('ufo_ir_parallel_geometry_new',
                                     'libufoir_parallel_geometry.so')

        projector = env.pm.get_plugin('ufo_ir_cl_projector_new',
                                      'libufoir_cl_projector.so')

        sart = env.pm.get_plugin('ufo_ir_sart_method_new',
                                 'libufoir_sart_method.so')

        geometry.set_properties(num_angles=n_proj, angle_step=theta * 180.0 / np.pi)
        projector.set_properties(model='Joseph')
        sart.set_properties(relaxation_factor=0.25, max_iterations=1)
        ir = Ir(geometry=geometry, projector=projector, method=sart)
        bffr = Buffer()

        for i, slce in enumerate(bffr(ir(sinogen(projections)))):
            volume[i,:,:] = slce[:,:]

        dset.logger.info("ufo-sart [ok]")
        return _set_data(dset, volume, overwrite)

    setattr(tomopy.xtomo.xtomo_dataset.XTomoDataset, 'ufo_sart', sart)
except ImportError:
    pass


setattr(tomopy.xtomo.xtomo_dataset.XTomoDataset, 'ufo_fbp', fbp)
setattr(tomopy.xtomo.xtomo_dataset.XTomoDataset, 'ufo_dfi', dfi)
