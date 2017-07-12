project('ufo', 'c',
    version: '0.13.0'
)

version = meson.project_version()
components = version.split('.')
version_major = components[0]
version_minor = components[1]
version_patch = components[2]

cc = meson.get_compiler('c')

gnome = import('gnome')

glib_dep = dependency('glib-2.0', version: '>= 2.28')
gio_dep = dependency('gio-2.0', version: '>= 2.28')
gobject_dep = dependency('gobject-2.0', version: '>= 2.28')
gmodule_dep = dependency('gmodule-2.0', version: '>= 2.28')
json_dep = dependency('json-glib-1.0', version: '>= 0.10.0')
python_dep = dependency('python', required: false)
zmq_dep = dependency('libzmq', required: false)

opencl_dep = declare_dependency(dependencies: cc.find_library('OpenCL'))

deps = [
    glib_dep,
    gio_dep,
    gobject_dep,
    gmodule_dep,
    json_dep,
    opencl_dep,
    python_dep,
    zmq_dep,
]

include_dir = include_directories('.')

plugindir = '@0@/@1@/ufo'.format(get_option('prefix'), get_option('libdir'))
kerneldir = '@0@/@1@/ufo'.format(get_option('prefix'), get_option('datadir'))
header_subdir = 'ufo-@0@'.format(version_major)

conf = configuration_data()
conf.set_quoted('UFO_PLUGIN_DIR', plugindir)
conf.set_quoted('UFO_KERNEL_DIR', kerneldir)
conf.set_quoted('UFO_VERSION', version)
conf.set('WITH_PYTHON', python_dep.found())
conf.set('WITH_ZMQ', zmq_dep.found())

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
