progs = ['ufo-launch',
         'ufo-query',
         'ufo-runjson',
         'ufod']

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

