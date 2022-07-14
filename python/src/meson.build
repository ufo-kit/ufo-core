pygobject_dep = dependency('pygobject-3.0', version : '>=3.2.2')
ufo_dep = dependency('ufo', version : '>=0.4.0')

incdir_numpy = run_command(py3,
  [
    '-c',
    'import numpy; print(numpy.get_include())'
  ],
  check: true
).stdout().strip()

inc_np = include_directories(incdir_numpy)
np_dep = declare_dependency(include_directories: inc_np)

src = ['ufo.c']

dependencies = [py3_dep, np_dep, ufo_dep, pygobject_dep]

mod = py3.extension_module('_ufo',
    sources : src,
    dependencies : dependencies,
    install : true,
    subdir : 'ufo'
)
