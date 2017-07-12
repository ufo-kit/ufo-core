progs = [['ufo-launch', ['ufo-launch.c']],
         ['ufo-query', ['ufo-query.c']],
         ['ufo-runjson', ['ufo-runjson.c']],
         ['ufod', ['ufod.c']]]

foreach p: progs
    executable(p[0],
        sources: p[1],
        include_directories: include_dir,
        dependencies: deps,
        link_with: lib,
        install: true
    )
endforeach

