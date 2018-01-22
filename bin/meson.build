progs = [
    'ufo-launch',
    'ufo-query',
    'ufo-runjson',
]

foreach prog: progs
    sources = [
        '@0@.c'.format(prog),
        enums_h
    ]

    executable(prog,
        sources: sources,
        include_directories: include_dir,
        dependencies: deps,
        link_with: lib,
        install: true
    )
endforeach

template_dir = join_paths(get_option('prefix'), get_option('datadir'), 'ufo', 'templates')
mkfilter = configuration_data()
mkfilter.set('CMAKE_INSTALL_TEMPLATEDIR', template_dir)

configure_file(
  input: 'ufo-mkfilter.in',
  output: 'ufo-mkfilter',
  configuration: mkfilter,
  install_dir: get_option('bindir'),
)

install_data(
  ['templates/ufo-task.c.in', 'templates/ufo-task.h.in'],
  install_dir: template_dir,
)
