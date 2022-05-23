sources = [
    'test-suite.c',
    'test-buffer.c',
    'test-graph.c',
    'test-node.c',
    'test-profiler.c',
    'test-max-input-nodes.cpp'
]

test('unit tests',
    executable('test-suite',
        sources: sources,
        dependencies: [deps, lib_dep],
        override_options : ['cpp_std=c++11']
    )
)
