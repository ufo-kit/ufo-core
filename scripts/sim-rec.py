#!/usr/bin/env python

import sys
import os
import argparse
import numpy as np
from gi.repository import Ufo


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Simulate online reconstruction', add_help=False)

    args = [('-w', '--width', 'Height of the sample plane', dict(type=int, required=True)),
            ('-h', '--height', 'Width of the sample plane', dict(type=int, required=True)),
            ('-n', '--num-iterations', 'Number of iterations', dict(type=int)),
            ('-o', '--output-path', 'Path to store slices', dict(default='./'))
        ]

    for a1, a2, desc, options in args:
        dest_string = a2[2:].replace('-', '_')
        parser.add_argument(a1, a2, help=desc, dest=dest_string, **options)

    args = parser.parse_args()
    w, h, n = args.width, args.height, args.num_iterations

    g = Ufo.Graph(paths=os.environ.get('UFO_FILTER_PATH'))

    try:
        meta_balls = g.get_filter('metaballs')
        fp = g.get_filter('forwardproject')
        fft = g.get_filter('fft')
        ifft = g.get_filter('ifft')
        fltr = g.get_filter('filter')
        bp = g.get_filter('backproject')
        writer = g.get_filter('writer')
    except RuntimeError as e:
        print e
        sys.exit()

    meta_balls.set_properties(width=w, height=h, num_balls=4, num_iterations=n)
    fp.set_properties(num_projections=w, angle_step=np.pi/w)
    fft.set_properties(dimensions=1)
    ifft.set_properties(dimensions=1, final_width=w)
    bp.set_properties(axis_pos=w/2.0 + 0.5, angle_step=np.pi/w)
    writer.set_properties(path=args.output_path, prefix='mb')
    
    meta_balls.connect_to(fp)
    fp.connect_to(fft)
    fft.connect_to(fltr)
    fltr.connect_to(ifft)
    ifft.connect_to(bp)
    bp.connect_to(writer)

    g.run()
