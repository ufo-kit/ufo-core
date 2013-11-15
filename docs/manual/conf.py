# -*- coding: utf-8 -*-

import time
import re


def get_version():
    patterns = [
            r'^set\(UFO_VERSION_MAJOR "(\d*)"\)',
            r'^set\(UFO_VERSION_MINOR "(\d*)"\)',
            r'^set\(UFO_VERSION_PATCH "(\d*)"\)'
            ]
    version = ["0", "0", "0"]

    with open('../../CMakeLists.txt', 'r') as f:
        lines = f.readlines()
        major_pattern = r'^set\(UFO_VERSION_MAJOR "(\d*)"\)'

        for line in lines:
            for i, pattern in enumerate(patterns):
                m = re.match(pattern, line)

                if m:
                    version[i] = m.group(1)

    return '.'.join(version)


project = u'UFO'
copyright = u'%s, UFO Development Team a Collaboration of KIT, SCI and TPU' % time.strftime('%Y')
version = get_version()
release = version

extensions = ['sphinx.ext.pngmath']
templates_path = ['_templates']

master_doc = 'index'
source_suffix = '.rst'
source_encoding = 'utf-8'

primary_domain = 'c'

add_function_parentheses = True
pygments_style = 'default'

html_theme = 'default'
html_logo = "_static/ufo-logo.png"
html_static_path = ['_static']
html_use_smartypants = True

htmlhelp_basename = 'UFOdoc'

latex_documents = [
  ('index', 'UFO.tex', u'UFO Documentation',
   u'Matthias Vogelgesang', 'manual'),
]
