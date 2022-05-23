progs = [
    'ufo-query',
    'ufo-runjson',
]

foreach prog: progs
    sources = [
        '@0@.c'.format(prog),
    ]

    executable(prog,
        sources: sources,
        include_directories: include_dir,
        dependencies: deps + [enums_dep],
        link_with: lib,
        install: true
    )
endforeach

ufo_launch = executable('ufo-launch',
    sources: ['ufo-launch.c'],
    include_directories: include_dir,
    dependencies: deps + [enums_dep],
    link_with: lib,
    install: true
)

completiondir = get_option('bashcompletiondir')

if completiondir == ''
    bash_completion = dependency('bash-completion', required: false)

    if bash_completion.found()
        completiondir = bash_completion.get_pkgconfig_variable('completionsdir')
    else
        completiondir = join_paths(datadir, 'bash-completion/completions')
    endif
endif

if completiondir != 'no'
    install_data('ufo-launch',
                 install_dir: completiondir)
endif

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
