kernel_files = [
    'arithmetics.cl',
    'backproject.cl',
    'binarize.cl',
    'bin.cl',
    'clip.cl',
    'complex.cl',
    'conebeam.cl',
    'correlate.cl',
    'cut.cl',
    'cut-sinogram.cl',
    'cumsum.cl',
    'denoise.cl',
    'dfi.cl',
    'edge.cl',
    'estimate-noise.cl',
    'ffc.cl',
    'fft.cl',
    'fftmult.cl',
    'filter.cl',
    'flip.cl',
    'forwardproject.cl',
    'gaussian.cl',
    'gradient.cl',
    'histthreshold.cl',
    'interpolator.cl',
    'mask.cl',
    'median.cl',
    'metaballs.cl',
    'morphology.cl',
    'nlm.cl',
    'ordfilt.cl',
    'opencl.cl',
    'opencl-reduce.cl',
    'pad.cl',
    'phase-retrieval.cl',
    'piv.cl',
    'polar.cl',
    'rescale.cl',
    'reductor.cl',
    'rm-outliers.cl',
    'rotate.cl',
    'segment.cl',
    'split.cl',
    'stacked-backproject.cl',
    'swap-quadrants.cl',
    'transpose.cl',
    'zeropad.cl',
    'templates/general_bp_definitions.in',
    'templates/general_bp_body.in',
    'templates/general_bp_header_scalar.in',
    'templates/general_bp_header_vector.in',
]

install_data(kernel_files,
    install_dir: kernel_install_dir,
)

# generate kernels for lamino_backproject
lamino_kernel_names = [
    'z',
    'lamino',
    'center',
    'roll',
]

if python.found()
    command = [
        python.path(),
        '@INPUT0@', '@INPUT1@',
        '1', '2', '4', '8', '16',
    ]

    foreach name: lamino_kernel_names
        target_name = '@0@_kernel'.format(name)
        template_name = 'templates/@0@_template.in'.format(name)
        output_name = '@0@_kernel.cl'.format(name)

        custom_target(target_name,
            build_by_default: true,
            capture: true,
            command: command,
            input: ['tools/make_burst_kernels.py', template_name],
            output: output_name,
            install: true,
            install_dir: kernel_install_dir
        )
    endforeach
endif
