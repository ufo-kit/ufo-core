import numpy as np
from gi.repository import Ufo


if __name__ == '__main__':
    total_width, total_height = (832, 400)
    num_projections = 300
    axis_pos, angle_step = (413., 0.01256637)
    num_chunks = 2

    g = Ufo.Graph()

    proj_reader = g.get_filter('reader')
    roi = g.get_filter('regionofinterest')
    sino_generator = g.get_filter('sinogenerator')
    fft = g.get_filter('fft')
    ifft = g.get_filter('ifft')
    fltr = g.get_filter('filter')
    bp = g.get_filter('backproject')
    slice_writer = g.get_filter('writer')

    proj_reader.set_properties(path='projections/*.tif')
    sino_generator.set_properties(num_projections=num_projections)
    slice_writer.set_properties(path='./tmp', prefix='slice')

    bp.set_properties(axis_pos=axis_pos, angle_step=angle_step)
    fft.set_properties(dimensions=1)
    ifft.set_properties(dimensions=1, final_width=total_width)

    proj_reader.connect_to(roi)
    roi.connect_to(sino_generator)
    sino_generator.connect_to(fft)
    fft.connect_to(fltr)
    fltr.connect_to(ifft)
    fft.connect_to(ifft)
    ifft.connect_to(bp)
    bp.connect_to(slice_writer)

    steps = total_height / num_chunks
    current_height = 0

    for i in range(num_chunks):
        slice_writer.set_properties(prefix=('slice%02i' % i))
        roi.set_properties(y=current_height, width=total_width, height=steps)
        current_height += steps - 1
        g.run()
