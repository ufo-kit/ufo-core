#!/usr/bin/python

"""
This file generates GObject file templates that a filter author can use, to
implement their own filters.
"""

import sys
import re
import string
import textwrap
import argparse

FILTER_TYPES = ['source', 'sink', 'process', 'reduce']

def type_list():
    return ', '.join(['`' + f + '\'' for f in FILTER_TYPES])

class FilternameAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if not re.match(r"^[A-Z][a-z]([A-Z][a-z0-9]*)*", values):
            raise argparse.ArgumentTypeError('Name must be a camel-cased C identifier')
        setattr(namespace, self.dest, values)


class TypeAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if values not in FILTER_TYPES:
            raise argparse.ArgumentTypeError('Type must be one of ' + type_list())
        setattr(namespace, self.dest, values)


def generate_file(name, ftype, suffix='h'):
    filter_camel = name
    filter_hyphen = name[0].lower() + name[1:]

    for letter in string.ascii_uppercase:
        filter_hyphen = filter_hyphen.replace(letter, "-" + letter.lower())

    filter_underscore = filter_hyphen.replace('-', '_')

    d = {'prefix_underscore': filter_underscore,
         'prefix_camel': name,
         'prefix_hyphen': filter_hyphen,
         'prefix_upper': filter_underscore.upper()}

    filename = "ufo-filter-%s.%s" % (filter_hyphen, suffix)

    with open('templates/ufo-filter-%s.%s.in' % (ftype, suffix)) as f:
        template = string.Template(f.read())
        source = template.substitute(d)

        file = open(filename, "w")
        file.writelines(source)
        file.close()
        print "Wrote %s" % filename


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate UfoFilter skeletons')
    parser.add_argument('--name', required=True, type=str,
            action=FilternameAction, help='Name of the new filter in CamelCase')
    parser.add_argument('--type', required=True, type=str, action=TypeAction,
            help='Type of the new filter [one of %s]' % type_list())

    try:
        args = parser.parse_args()
    except argparse.ArgumentTypeError as err:
        print err
        sys.exit(1)

    generate_file(args.name, args.type, 'h')
    generate_file(args.name, args.type, 'c')

    message = "If you are about to write a UFO internal filter, you should copy \
the generated files into core/filters and adapt the CMakeLists.txt file. You \
should only add the filter sources to ${ufo_SRCS} if all build dependencies are \
met for your particular plugin.  Good luck!"
    
    print ""
    print textwrap.fill(message)
