#!/usr/bin/env python

import sys
import os
import argparse
from gi.repository import Ufo

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compute the center of rotation using image registration')
    parser.add_argument('-p', '--projection-path', dest='projection_path',
            type=str, default='./', metavar='PATH')

    args = parser.parse_args()

    g = Ufo.Graph(paths=os.environ.get('UFO_FILTER_PATH'))

    try:
        proj_reader = g.get_filter('reader')
        cor = g.get_filter('centerofrotation')
    except RuntimeError as e:
        print e
        sys.exit()

    proj_reader.set_properties(path=args.projection_path)
    cor.set_properties(use_sinograms=True)

    proj_reader.connect_to(cor)

    g.run()
