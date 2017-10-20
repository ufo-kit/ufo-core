sources = [
    'test-suite.c',
    'test-buffer.c',
    'test-graph.c',
    'test-node.c',
    'test-profiler.c',
]

test('all tests',
    executable('test-suite',
        sources: sources + [enums_h],
        include_directories: include_dir,
        dependencies: deps,
        link_with: lib,
    )
)
