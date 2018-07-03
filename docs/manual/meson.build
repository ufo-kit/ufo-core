sources = [
    'bugs.rst',
    'changelog.rst',
    'copyright.rst',
    'devel.rst',
    'faq.rst',
    'index.rst',
    'install/docker.rst',
    'install/index.rst',
    'install/linux.rst',
    'install/mac.rst',
    'using/api.rst',
    'using/env.rst',
    'using/execution.rst',
    'using/index.rst',
    'using/json.rst',
    'using/quickstart.rst',
]

sphinx = find_program('sphinx-build', required: false)

if sphinx.found()
    command = [
        sphinx.path(),
        '-q',
        '-b', 'html',
        meson.current_source_dir(),
        meson.current_build_dir(),
    ]

    manual = custom_target('manual',
        build_by_default: true,
        command: command,
        input: sources,
        output: 'index.html',
    )

    install_subdir(meson.current_build_dir(), install_dir: docdir)
else
    message('sphinx not found, cannot generate manual.')
endif

