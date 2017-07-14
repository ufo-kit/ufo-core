progs = ['ufo-launch',
         'ufo-query',
         'ufo-runjson',
         'ufod']

foreach prog: progs
    executable(prog,
        '@0@.c'.format(prog),
        include_directories: include_dir,
        dependencies: deps,
        link_with: lib,
        install: true
    )
endforeach

