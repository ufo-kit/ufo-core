#!/usr/bin/env python

import sys
import os
import argparse
import numpy as np
from gi.repository import Ufo

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compute the filtered back-projection on a set of sinograms.')

    args = [('-a', '--axis-pos', 'Position of rotation axis', dict(type=float, required=True)),
            ('-s', '--sinogram-path', 'Wildcarded path to sinograms', dict(required=True)),
            ('-n', '--num-projections', 'Number of projections', dict(type=int)),
            ('-t', '--angle-step', 'Rotation angle', dict(type=float)),
            ('-o', '--output-path', 'Path to store slices', dict(default='./'))
        ]

    for a1, a2, desc, options in args:
        dest_string = a2[2:].replace('-', '_')
        parser.add_argument(a1, a2, help=desc, dest=dest_string, **options)

    args = parser.parse_args()
    g = Ufo.Graph(paths=os.environ.get('UFO_FILTER_PATH'))

    try:
        sino_reader = g.get_filter('reader')
        fft = g.get_filter('fft')
        ifft = g.get_filter('ifft')
        fltr = g.get_filter('filter')
        bp = g.get_filter('backproject')
        slice_writer = g.get_filter('writer')
    except RuntimeError as e:
        print e
        sys.exit()

    sino_reader.set_properties(path=args.sinogram_path)
    slice_writer.set_properties(path=args.output_path, prefix='slice')

    # if only number of projection is given, estimate the rotation angle
    angle_step = args.angle_step if args.angle_step else np.pi / args.num_projections

    bp.set_properties(axis_pos=args.axis_pos, angle_step=angle_step)
    fft.set_properties(dimensions=1)
    ifft.set_properties(dimensions=1)

    sino_reader.connect_to(fft)
    fft.connect_to(fltr)
    fltr.connect_to(ifft)
    ifft.connect_to(bp)
    bp.connect_to(slice_writer)

    g.run()
