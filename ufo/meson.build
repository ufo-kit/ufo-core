sources = [
    'compat.c',
    'ufo-base-scheduler.c',
    'ufo-basic-ops.c',
    'ufo-buffer.c',
    'ufo-copy-task.c',
    'ufo-copyable-iface.c',
    'ufo-cpu-node.c',
    'ufo-daemon.c',
    'ufo-dummy-task.c',
    'ufo-fixed-scheduler.c',
    'ufo-gpu-node.c',
    'ufo-graph.c',
    'ufo-group.c',
    'ufo-group-scheduler.c',
    'ufo-input-task.c',
    'ufo-local-scheduler.c',
    'ufo-messenger-iface.c',
    'ufo-method-iface.c',
    'ufo-node.c',
    'ufo-output-task.c',
    'ufo-plugin-manager.c',
    'ufo-priv.c',
    'ufo-profiler.c',
    'ufo-processor.c',
    'ufo-remote-node.c',
    'ufo-remote-task.c',
    'ufo-resources.c',
    'ufo-scheduler.c',
    'ufo-task-iface.c',
    'ufo-task-graph.c',
    'ufo-task-node.c',
    'ufo-transform-iface.c',
    'ufo-two-way-queue.c',
]

headers = [
    'ufo.h',
    'ufo-base-scheduler.h',
    'ufo-basic-ops.h',
    'ufo-buffer.h',
    'ufo-copy-task.h',
    'ufo-copyable-iface.h',
    'ufo-cpu-node.h',
    'ufo-daemon.h',
    'ufo-dummy-task.h',
    'ufo-fixed-scheduler.h',
    'ufo-gpu-node.h',
    'ufo-graph.h',
    'ufo-group.h',
    'ufo-group-scheduler.h',
    'ufo-input-task.h',
    'ufo-local-scheduler.h',
    'ufo-messenger-iface.h',
    'ufo-method-iface.h',
    'ufo-node.h',
    'ufo-output-task.h',
    'ufo-plugin-manager.h',
    'ufo-profiler.h',
    'ufo-processor.h',
    'ufo-remote-node.h',
    'ufo-remote-task.h',
    'ufo-resources.h',
    'ufo-scheduler.h',
    'ufo-task-iface.h',
    'ufo-task-graph.h',
    'ufo-task-node.h',
    'ufo-transform-iface.h',
    'ufo-two-way-queue.h',
]

enums = gnome.mkenums('ufo-enums',
    sources: headers,
    h_template: 'ufo-enums.h.template',
    c_template: 'ufo-enums.c.template',
    install_header: true,
    install_dir: '@0@/@1@'.format(get_option('includedir'), header_subdir),
)

enums_c = enums[0]
enums_h = enums[1]

sources += [enums_c, enums_h]

if zmq_dep.found()
    sources += ['ufo-zmq-messenger.c']
endif

m_dep = declare_dependency(
    dependencies: cc.find_library('m')
)

lib = shared_library('ufo',
    sources: sources,
    dependencies: deps + [m_dep],
    version: version,
    soversion: version_major,
    include_directories: include_dir,
    install: true,
)

gnome.generate_gir(lib,
    namespace: 'Ufo',
    nsversion: '@0@.0'.format(version_major),
    sources: sources + headers,
    install: true,
    includes: [
      'GLib-2.0',
      'GObject-2.0',
      'GModule-2.0',
      'Json-1.0',
    ],
)

pkg = import('pkgconfig')

pkg.generate(
    libraries: [lib],
    version: version,
    name: 'ufo',
    description: 'Library for unified scientific camera access',
    requires: [
        'glib-2.0',
        'gobject-2.0',
        'gmodule-2.0',
        'gio-2.0',
        'json-glib-1.0',
    ],
    variables: [
        'plugindir=${libdir}/ufo',
        'kerneldir=${prefix}/@0@/ufo'.format(get_option('datadir')),
    ],
    subdirs: [
        header_dir
    ],
)

install_headers(headers, subdir: header_subdir)
