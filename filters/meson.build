plugins = [
    'average',
    'backproject',
    'bin',
    'binarize',
    'blur',
    'buffer',
    'calculate',
    'clip',
    'crop',
    'cut',
    'cut-sinogram',
    'center-of-rotation',
    'concatenate-result',
    'contrast',
    'correlate-stacks',
    'dfi-sinc',
    'denoise',
    'detect-edge',
    'rescale',
    'dummy-data',
    'dump-ring',
    'duplicate',
    'filter',
    'flatten',
    'flatten-inplace',
    'flat-field-correct',
    'fftmult',
    'filter-particle',
    'filter-stripes',
    'filter-stripes1d',
    'flip',
    'forwardproject',
    'get-dup-circ',
    'gradient',
    'horizontal-interpolate',
    'interpolate',
    'interpolate-stream',
    'loop',
    'map-slice',
    'map-color',
    'mask',
    'measure',
    'measure-sharpness',
    'median-filter',
    'memory-in',
    'memory-out',
    'merge',
    'metaballs',
    'monitor',
    'null',
    'opencl',
    'opencl-reduce',
    'ordfilt',
    'pad',
    'polar-coordinates',
    'power-spectrum',
    'reduce',
    'refeed',
    'remove-circle',
    'ring-pattern',
    'ringwriter',
    'replicate',
    'remove-outliers',
    'rotate',
    'segment',
    'sleep',
    'slice',
    'sliding-stack',
    'stack',
    'stacked-backproject',
    'stitch',
    'stdin',
    'tile',
    'transpose',
    'transpose-projections',
    'swap-quadrants',
    'subtract',
    'unsplit',
    'volume-render',
    'zeropad',
]

fft_plugins = [
    'fft',
    'ifft',
    'retrieve-phase',
]

zmq_plugins = [
    'zmq-pub',
    'zmq-sub',
]

kernels = [
]

read_sources = [
    'ufo-read-task.c',
    'readers/ufo-reader.c',
    'readers/ufo-edf-reader.c',
    'readers/ufo-raw-reader.c',
]

write_sources = [
    'ufo-write-task.c',
    'writers/ufo-writer.c',
    'writers/ufo-raw-writer.c',
]

tiff_dep = dependency('libtiff-4', required: false)
hdf5_dep = dependency('hdf5', required: false)
jpeg_dep = dependency('libjpeg', required: false)
pangocairo_dep = dependency('pangocairo', required: false)
opencv_dep = dependency('opencv', required: false)
uca_dep = dependency('libuca', required: false)
clblast_dep = dependency('clblast', required: false)
clfft_dep = dependency('clFFT', required: false)
zmq_dep = dependency('libzmq', required: false)
json_dep = dependency('json-glib-1.0', version: '>=1.1.0', required: false)

conf = configuration_data()
conf.set('HAVE_AMD', clfft_dep.found())
conf.set('HAVE_TIFF', tiff_dep.found())
conf.set('HAVE_JPEG', jpeg_dep.found())
conf.set('WITH_HDF5', hdf5_dep.found())
conf.set('BURST', get_option('lamino_backproject_burst_mode'))
conf.set('CL_TARGET_OPENCL_VERSION', '120')

configure_file(
    input: 'config.h.meson.in',
    output: 'config.h',
    configuration: conf,
)

# standard plugins

foreach plugin: plugins
    shared_module(plugin,
        'ufo-@0@-task.c'.format(plugin),
        dependencies: deps,
        name_prefix: 'libufofilter',
        install: true,
        install_dir: plugin_install_dir,
    )
endforeach

# generalized backproject and conebeam

shared_module('general-backproject',
    sources: [
        'ufo-general-backproject-task.c',
        'common/ufo-conebeam.c',
        'common/ufo-ctgeometry.c',
        'common/ufo-math.c',
        'common/ufo-scarray.c',
    ],
    dependencies: deps,
    name_prefix: 'libufofilter',
    install: true,
    install_dir: plugin_install_dir,
)

shared_module('cone-beam-projection-weight',
    sources: [
        'ufo-cone-beam-projection-weight-task.c',
        'common/ufo-conebeam.c',
        'common/ufo-scarray.c',
    ],
    dependencies: deps,
    name_prefix: 'libufofilter',
    install: true,
    install_dir: plugin_install_dir,
)

# non local means

shared_module('non-local-means',
    sources: [
        'ufo-non-local-means-task.c',
        'common/ufo-math.c',
        'common/ufo-common.c',
    ],
    dependencies: deps,
    name_prefix: 'libufofilter',
    install: true,
    install_dir: plugin_install_dir,
)

# find large spots

shared_module('find-large-spots',
    sources: [
        'ufo-find-large-spots-task.c',
        'common/ufo-math.c',
        'common/ufo-common.c',
    ],
    dependencies: deps,
    name_prefix: 'libufofilter',
    install: true,
    install_dir: plugin_install_dir,
)

# fft plugins

have_clfft = clfft_dep.found()
with_oclfft = get_option('oclfft')

if have_clfft or with_oclfft
    fft_deps = deps

    if clfft_dep.found()
        fft_deps += [clfft_dep]
    else
        fft_deps += [oclfft_dep]
    endif

    common_fft = static_library('commonfft',
        'common/ufo-fft.c',
        dependencies: fft_deps,
    )

    foreach plugin: fft_plugins
        shared_module(plugin,
            'ufo-@0@-task.c'.format(plugin),
            dependencies: deps,
            name_prefix: 'libufofilter',
            link_with: common_fft,
            install: true,
            install_dir: plugin_install_dir,
        )
    endforeach
endif

# lamino plugin

python = find_program('python', required: false)

if python.found()
    shared_module('lamino-backproject',
        sources: ['ufo-lamino-backproject-task.c'],
        dependencies: deps,
        name_prefix: 'libufofilter',
        install: true,
        install_dir: plugin_install_dir,
    )
else
    message('Python not found, cannot generate kernels for lamino_backproject')
endif

# i/o plugins

read_deps = deps
write_deps = deps

if tiff_dep.found()
    read_sources += ['readers/ufo-tiff-reader.c']
    read_deps += [tiff_dep]

    write_sources += ['writers/ufo-tiff-writer.c']
    write_deps += [tiff_dep]
endif

if hdf5_dep.found()
    read_sources += ['readers/ufo-hdf5-reader.c', 'common/hdf5.c']
    read_deps += [hdf5_dep]

    write_sources += ['writers/ufo-hdf5-writer.c', 'common/hdf5.c']
    write_deps += [hdf5_dep]
endif

if jpeg_dep.found()
    write_sources += ['writers/ufo-jpeg-writer.c']
    write_deps += [jpeg_dep]
endif

shared_module('read',
    sources: read_sources,
    dependencies: read_deps,
    name_prefix: 'libufofilter',
    install: true,
    install_dir: plugin_install_dir,
)

shared_module('write',
    sources: write_sources,
    dependencies: write_deps,
    name_prefix: 'libufofilter',
    install: true,
    install_dir: plugin_install_dir,
)

# camera

if uca_dep.found()
    shared_module('camera',
        sources: ['ufo-camera-task.c'],
        dependencies: deps + [uca_dep],
        name_prefix: 'libufofilter',
        install: true,
        install_dir: plugin_install_dir,
    )
endif

# cvshow

if opencv_dep.found()
    shared_module('cv-show',
        sources: ['writers/ufo-writer.c', 'ufo-cv-show-task.cpp'],
        dependencies: deps + [opencv_dep],
        name_prefix: 'libufofilter',
        install: true,
        install_dir: plugin_install_dir,
    )
endif

# stamp

if pangocairo_dep.found()
    shared_module('stamp', 'ufo-stamp-task.c',
        dependencies: deps + [pangocairo_dep],
        name_prefix: 'libufofilter',
        install: true,
        install_dir: plugin_install_dir,
    )
endif

# gemm

if clblast_dep.found()
    shared_module('gemm', 'ufo-gemm-task.c',
        dependencies: deps + [clblast_dep],
        name_prefix: 'libufofilter',
        install: true,
        install_dir: plugin_install_dir,
    )
endif

# zmq-sub/zmq-pub

if zmq_dep.found() and json_dep.found()
    foreach plugin: zmq_plugins

        shared_module(plugin,
            'ufo-@0@-task.c'.format(plugin),
            dependencies: deps + [zmq_dep, json_dep],
            name_prefix: 'libufofilter',
            install: true,
            install_dir: plugin_install_dir,
        )
    endforeach
endif

subdir('kernels')
