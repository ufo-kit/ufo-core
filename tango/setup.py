from setuptools import setup

setup(
    name='tangofu',
    version='0.0.1',
    author='Matthias Vogelgesang',
    author_email='matthias.vogelgesang@kit.edu',
    url='http://ufo.kit.edu',
    license='(?)',
    description='TANGO server for UFO',
    long_description='TANGO server for UFO',
    scripts=['Ufo'],
    install_requires=['PyTango']
)
