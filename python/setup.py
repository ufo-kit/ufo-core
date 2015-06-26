import os
import sys
import subprocess
import pkgconfig
import numpy
from setuptools import setup, Extension


VERSION = '1.1.0'


if not pkgconfig.installed('pygobject-3.0', '>=3.2.2') and \
   not pkgconfig.installed('pygobject-2.0', '>=2.28'):
    sys.exit("You must install pygobject-2.0 or pygobject-3.0")

if not pkgconfig.installed('ufo', '>=0.4.0'):
    sys.exit("You must install ufo>=0.4.0")

def listify(d):
    return {k: list(v) for k, v in d.items()}

build_flags = listify(pkgconfig.parse('pygobject-2.0 pygobject-3.0 ufo'))

build_flags['include_dirs'].append(numpy.get_include())
build_flags['extra_compile_args'] = ['-std=c99']

setup(
    name='ufo',
    version=VERSION,
    author='Matthias Vogelgesang',
    author_email='matthias.vogelgesang@kit.edu',
    url='http://ufo.kit.edu',
    license='GPL v3 (?)',
    description='ufo extension module',
    long_description='ufo extension module',
    packages=['ufo'],
    ext_modules=[Extension('_ufo', ['src/ufo.c'], **build_flags)]
)
