#!/usr/bin/python

"""
This file generates GObject file templates that a filter author can use, to
implement their own nodes.
"""

import os
import sys
import re
import string
import textwrap
import argparse
import jinja2

def type_list():
    return ', '.join(['`' + f + '\'' for f in FILTER_TYPES])

class FilternameAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if not re.match(r"^[A-Z][a-z]([A-Z][a-z0-9]*)*", values):
            raise argparse.ArgumentTypeError('Name must be a camel-cased C identifier')
        setattr(namespace, self.dest, values)


class TypeAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        lower = values.lower()
        if lower not in ['gpu', 'cpu']:
            raise argparse.ArgumentTypeError("Type must be either `cpu' or `gpu'")
        setattr(namespace, self.dest, values)


def generate_file(args, env, suffix='h'):
    camelcased = args.name
    hyphenated = args.name[0].lower() + args.name[1:]

    for letter in string.ascii_uppercase:
        hyphenated = hyphenated.replace(letter, "-" + letter.lower())

    underscored = hyphenated.replace('-', '_')
    uppercased = underscored.upper()

    template = env.get_template('ufo-%s-task.%s.in' % (args.type, suffix))
    res = template.render(camelcased=camelcased,
                          uppercased=uppercased,
                          hyphenated=hyphenated,
                          underscored=underscored,
                          args=args)

    filename = "ufo-%s-task.%s" % (hyphenated, suffix)

    with open(filename, 'w') as f:
        f.writelines(res)
        f.close()
        print "Wrote %s" % filename


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate UfoNode skeletons')
    parser.add_argument('-n', '--name', required=True, type=str,
                        action=FilternameAction,
                        help='Name of the new filter in CamelCase')
    parser.add_argument('-t', '--type', type=str, default='cpu',
                        action=TypeAction,
                        help="Either `cpu' or `gpu' (cpu is default)")
    parser.add_argument('-d', '--disable-comments',
                        action='store_false',
                        help='Do not insert comments into source files')

    try:
        args = parser.parse_args()
    except argparse.ArgumentTypeError as err:
        print err
        sys.exit(1)

    template_dir = '@UFO_FILTER_TEMPLATE_DIR@'

    if not os.path.exists(template_dir):
        template_dir = 'templates'

    loader = jinja2.FileSystemLoader(template_dir)
    env = jinja2.Environment(loader=loader)

    generate_file(args, env, 'h')
    generate_file(args, env, 'c')

    message = "If you are about to write a UFO internal filter, you should copy \
the generated files into core/filters and adapt the CMakeLists.txt file. You \
should only add the filter sources to ${ufo_SRCS} if all build dependencies are \
met for your particular plugin.  Good luck!"

    print ""
    print textwrap.fill(message)
