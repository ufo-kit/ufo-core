project('ufo', ['c', 'cpp'],
    version: '0.17.0',
    default_options: ['c_std=gnu99'],
)

version = meson.project_version()
components = version.split('.')
version_major = components[0]
version_minor = components[1]
version_patch = components[2]

cc = meson.get_compiler('c')

gnome = import('gnome')

glib_dep = dependency('glib-2.0', version: '>= 2.38')
gio_dep = dependency('gio-2.0', version: '>= 2.38')
gobject_dep = dependency('gobject-2.0', version: '>= 2.38')
gmodule_dep = dependency('gmodule-2.0', version: '>= 2.38')
json_dep = dependency('json-glib-1.0', version: '>= 0.10.0')

pymod = import('python')
python = pymod.find_installation(get_option('python'))
python_dep = python.dependency(required: false, embed: true)

opencl_dep = dependency('OpenCL', required: false)

if not opencl_dep.found()
    if not cc.has_header('CL/cl.h')
        error('Cannot find CL/cl.h')
    endif

    opencl_dep = declare_dependency(dependencies: cc.find_library('OpenCL'))
endif

deps = [
    glib_dep,
    gio_dep,
    gobject_dep,
    gmodule_dep,
    json_dep,
    opencl_dep,
    python_dep,
]

include_dir = include_directories('.')

prefixdir = get_option('prefix')
datadir = join_paths(prefixdir, get_option('datadir'))
docdir = join_paths(datadir, 'doc', 'ufo')
plugindir = join_paths(prefixdir, get_option('libdir'), 'ufo')
kerneldir = join_paths(datadir, 'ufo')
header_dir = 'ufo-@0@'.format(version_major)
header_subdir = join_paths(header_dir, 'ufo')

conf = configuration_data()
conf.set_quoted('UFO_PLUGIN_DIR', plugindir)
conf.set_quoted('UFO_KERNEL_DIR', kerneldir)
conf.set_quoted('UFO_VERSION', version)
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_38')
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_38')
conf.set('CL_TARGET_OPENCL_VERSION', '120')
conf.set('UFO_MAX_INPUT_NODES', get_option('ufo_max_input_nodes'))

if python_dep.found()
  conf.set('WITH_PYTHON', true)
endif

configure_file(
    input: 'config.h.meson.in',
    output: 'config.h',
    configuration: conf
)

add_global_arguments('-DUFO_COMPILATION', language: 'c')
add_global_arguments('-DCL_USE_DEPRECATED_OPENCL_1_1_APIS', language: 'c')
add_global_arguments('-DCL_USE_DEPRECATED_OPENCL_1_2_APIS', language: 'c')
add_global_arguments('-DGLIB_DISABLE_DEPRECATION_WARNINGS', language: 'c')

subdir('ufo')
subdir('bin')

if get_option('gtk_doc')
  gtk_doc_dep = dependency('gtk-doc', required: false)
  if gtk_doc_dep.found()
    subdir('docs')
  endif
endif

if get_option('with_tests')
    subdir('tests')
endif
