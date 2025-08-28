# Copyright (C) 2016 Karlsruhe Institute of Technology
#
# This file is part of Ufo.
#
# This library is free software: you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library.  If not, see <http://www.gnu.org/licenses/>.

"""Generate burst laminographic backprojection OpenCL kernels."""
import argparse
import os


IDX_TO_VEC_ELEM = dict(zip(range(10), range(10)))
IDX_TO_VEC_ELEM[10] = 'a'
IDX_TO_VEC_ELEM[11] = 'b'
IDX_TO_VEC_ELEM[12] = 'c'
IDX_TO_VEC_ELEM[13] = 'd'
IDX_TO_VEC_ELEM[14] = 'e'
IDX_TO_VEC_ELEM[15] = 'f'


def fill_compute_template(tmpl, num_items, index):
    """Fill the template doing the pixel computation and texture fetch."""
    operation = '+' if index else ''
    access = '.s{}'.format(IDX_TO_VEC_ELEM[index]) if num_items > 1 else ''
    return tmpl.format(index, access, operation)


def fill_kernel_template(input_tmpl, compute_tmpl, kernel_outer, kernel_inner, num_items):
    """Construct the whole kernel."""
    vector_length = num_items if num_items > 1 else ''
    computes = '\n'.join([fill_compute_template(compute_tmpl, num_items, i)
                          for i in range(num_items)])
    inputs = '\n'.join([input_tmpl.format(i) for i in range(num_items)])
    kernel_inner = kernel_inner.format(computes)

    return kernel_outer.format(num_items, inputs, vector_length, kernel_inner)


def parse_args():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument('filename', type=str, help='File name with the kernel template')
    parser.add_argument('burst', type=int, nargs='+',
                        help='Number of projections processed by one kernel invocation')

    return parser.parse_args()


def main():
    """execute program."""
    args = parse_args()
    allowed_bursts = [2 ** i for i in range(5)]
    in_tmpl = "read_only image2d_t projection_{},"
    common_filename = os.path.join(os.path.dirname(args.filename), 'common.in')
    defs_filename = os.path.join(os.path.dirname(args.filename), 'definitions.in')
    defs = open(defs_filename, 'r').read()
    kernel_outer = open(common_filename, 'r').read()
    comp_tmpl, kernel_inner = open(args.filename, 'r').read().split('\n%nl\n')
    kernels = defs + '\n'
    for burst in args.burst:
        if burst not in allowed_bursts:
            raise ValueError('Burst mode `{}` must be one of `{}`'.format(burst, allowed_bursts))
        kernels += fill_kernel_template(in_tmpl, comp_tmpl, kernel_outer, kernel_inner, burst)

    print(kernels)


if __name__ == '__main__':
    main()
