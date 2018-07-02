a2x = find_program(['a2x', 'a2x.py'], required: false)

manpage_names = [
    'ufo-launch.1',
    'ufo-mkfilter.1',
    'ufo-query.1',
    'ufo-prof.1',
    'ufo-runjson.1',
]

if a2x.found()
    foreach manpage: manpage_names
        source = '@0@.txt'.format(manpage)
        command = [a2x.path()]
        command += [
            '-L', '-d', 'manpage', '-f', 'manpage',
            '-D', '@OUTDIR@',
            '@INPUT@'
            ]

        man = custom_target(manpage,
            build_by_default: true,
            command: command,
            input: source,
            output: manpage,
            install: true,
            install_dir: join_paths(get_option('mandir'), 'man1'),
        )
    endforeach
else
    message('a2x not found, cannot generate manpages.')
endif

