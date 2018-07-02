ignore_headers = [
    'ufo-priv.h',
    'ufo-two-way-queue.h',
]

if gtk_doc_dep.found() and get_option('gtk_doc')
    gnome.gtkdoc('Ufo',
        main_xml: 'Ufo-docs.xml',
        dependencies: lib_dep,
        gobject_typesfile: 'Ufo.types',
        src_dir: join_paths(meson.source_root(), 'ufo'),
        scan_args: [
          '--ignore-headers=' + ' '.join(ignore_headers),
        ],
        install: true,
    )
endif

subdir('man')
subdir('manual')
