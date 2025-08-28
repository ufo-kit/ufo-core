cmake_minimum_required(VERSION 2.6)

#{{{ Sources
set(ufofilter_SRCS
    ufo-average-task.c
    ufo-backproject-task.c
    ufo-bin-task.c
    ufo-binarize-task.c
    ufo-blur-task.c
    ufo-buffer-task.c
    ufo-calculate-task.c
    ufo-clip-task.c
    ufo-crop-task.c
    ufo-cut-task.c
    ufo-cut-sinogram-task.c
    ufo-center-of-rotation-task.c
    ufo-concatenate-result-task.c
    ufo-cone-beam-projection-weight-task.c
    ufo-contrast-task.c
    ufo-correlate-stacks-task.c
    ufo-dfi-sinc-task.c
    ufo-denoise-task.c
    ufo-detect-edge-task.c
    ufo-rescale-task.c
    ufo-dummy-data-task.c
    ufo-dump-ring-task.c
    ufo-duplicate-task.c
    ufo-filter-task.c
    ufo-find-large-spots-task.c
    ufo-flatten-task.c
    ufo-flatten-inplace-task.c
    ufo-flat-field-correct-task.c
    ufo-fft-task.c
    ufo-fftmult-task.c
    ufo-filter-particle-task.c
    ufo-filter-stripes-task.c
    ufo-filter-stripes1d-task.c
    ufo-flip-task.c
    ufo-forwardproject-task.c
    ufo-get-dup-circ-task.c
    ufo-gradient-task.c
    ufo-general-backproject-task.c
    ufo-horizontal-interpolate-task.c
    ufo-ifft-task.c
    ufo-interpolate-task.c
    ufo-interpolate-stream-task.c
    ufo-lamino-backproject-task.c
    ufo-loop-task.c
    ufo-map-color-task.c
    ufo-map-slice-task.c
    ufo-mask-task.c
    ufo-measure-task.c
    ufo-measure-sharpness-task.c
    ufo-median-filter-task.c
    ufo-memory-in-task.c
    ufo-memory-out-task.c
    ufo-merge-task.c
    ufo-metaballs-task.c
    ufo-monitor-task.c
    ufo-non-local-means-task.c
    ufo-null-task.c
    ufo-opencl-task.c
    ufo-opencl-reduce-task.c
    ufo-ordfilt-task.c
    ufo-pad-task.c
    ufo-polar-coordinates-task.c
    ufo-power-spectrum-task.c
    ufo-read-task.c
    ufo-reduce-task.c
    ufo-refeed-task.c
    ufo-remove-circle-task.c
    ufo-remove-outliers-task.c
    ufo-retrieve-phase-task.c
    ufo-ring-pattern-task.c
    ufo-ringwriter-task.c
    ufo-replicate-task.c
    ufo-rotate-task.c
    ufo-sleep-task.c
    ufo-slice-task.c
    ufo-sliding-stack-task.c
    ufo-stack-task.c
    ufo-stacked-backproject-task.c
    ufo-stdin-task.c
    ufo-stitch-task.c
    ufo-tile-task.c
    ufo-transpose-task.c
    ufo-transpose-projections-task.c
    ufo-swap-quadrants-task.c
    ufo-subtract-task.c
    ufo-volume-render-task.c
    ufo-write-task.c
    ufo-zeropad-task.c
    )

set(ufoaux_SRCS
    ufo-priv.c)

set(read_aux_SRCS
    readers/ufo-reader.c
    readers/ufo-edf-reader.c
    readers/ufo-raw-reader.c)

set(write_aux_SRCS
    writers/ufo-writer.c
    writers/ufo-raw-writer.c)

set(stdout_aux_SRCS
    writers/ufo-writer.c)

set(filter_aux_SRCS
    common/ufo-fft.c)

set(fft_aux_SRCS
    common/ufo-fft.c)

set(ifft_aux_SRCS
    common/ufo-fft.c)

set(retrieve_phase_aux_SRCS
    common/ufo-fft.c)

set(lamino_backproject_aux_SRCS
    lamino-roi.c)

set(cone_beam_projection_weight_aux_SRCS
    common/ufo-scarray.c)

set(general_backproject_aux_SRCS
    common/ufo-math.c
    common/ufo-conebeam.c
    common/ufo-scarray.c
    common/ufo-ctgeometry.c)

set(non_local_means_aux_SRCS
    common/ufo-math.c
    common/ufo-common.c)

set(find_large_spots_aux_SRCS
    common/ufo-math.c
    common/ufo-common.c)

file(GLOB ufofilter_KERNELS "kernels/*.cl")
#}}}
#{{{ Variables
set(ufofilter_LIBS
    m
    ${UFO_LIBRARIES}
    ${OpenCL_LIBRARIES})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -pedantic -Wall -Wextra -fPIC -Wno-unused-parameter -Wno-deprecated-declarations")
set(CL_TARGET_OPENCL_VERSION "120")

add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGE_FILES)
#}}}
#{{{ Dependency checks
find_package(TIFF)
find_package(HDF5 1.8)
find_package(JPEG)
find_package(OpenMP)
find_package(OpenCV)

pkg_check_modules(UCA libuca>=1.2)
pkg_check_modules(LIBTIFF4 libtiff-4>=4.0.0)
pkg_check_modules(CLFFT clFFT)
pkg_check_modules(CLBLAST clblast)
pkg_check_modules(PANGOCAIRO pangocairo)
pkg_check_modules(OPENCV opencv)
pkg_check_modules(ZMQ libzmq)
pkg_check_modules(JSON_GLIB json-glib-1.0)


if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
endif ()

if (OpenCL_VERSION_MAJOR)
    if (${OpenCL_VERSION_MAJOR} GREATER 1 OR
        (${OpenCL_VERSION_MAJOR} EQUAL 1 AND ${OpenCL_VERSION_MINOR} GREATER 1))
        # We need OpenCL > 1.1 for clEnqueueFillBuffer.
        list(APPEND ufofilter_SRCS ufo-segment-task.c)
    endif ()
endif ()

if (TIFF_FOUND)
    list(APPEND read_aux_SRCS readers/ufo-tiff-reader.c)
    list(APPEND read_aux_LIBS ${TIFF_LIBRARIES})
    list(APPEND write_aux_SRCS writers/ufo-tiff-writer.c)
    list(APPEND write_aux_LIBS ${TIFF_LIBRARIES})
    include_directories(${TIFF_INCLUDE_DIRS})
    link_directories(${TIFF_LIBRARY_DIRS})
    set(HAVE_TIFF True)
elseif (LIBTIFF4_INCLUDE_DIRS AND LIBTIFF4_LIBRARIES)
    list(APPEND read_aux_SRCS readers/ufo-tiff-reader.c)
    list(APPEND read_aux_LIBS ${LIBTIFF4_LIBRARIES})
    list(APPEND write_aux_SRCS writers/ufo-tiff-writer.c)
    list(APPEND write_aux_LIBS ${LIBTIFF4_LIBRARIES})
    include_directories(${LIBTIFF4_INCLUDE_DIRS})
    link_directories(${LIBTIFF4_LIBRARY_DIRS})
    set(HAVE_TIFF True)
endif ()

if (JPEG_FOUND)
    list(APPEND write_aux_SRCS writers/ufo-jpeg-writer.c)
    list(APPEND write_aux_LIBS ${JPEG_LIBRARIES})
    include_directories(${JPEG_INCLUDE_DIRS})
    link_directories(${JPEG_LIBRARY_DIRS})
    set(HAVE_JPEG True)
endif ()

if (HDF5_FOUND)
    option(WITH_HDF5 "Use HDF5" ON)
    if (WITH_HDF5)
        # This hack is proudly presented by stupid CMake configure scripts and a
        # badly written libhdf5.
        find_path(MPI_INCLUDE_PATH NAMES mpi.h PATHS /usr/include /usr/include/mpich2 /usr/lib/openmpi/include)

        if (MPI_INCLUDE_PATH)
            include_directories(${MPI_INCLUDE_PATH})
        endif ()

        list(APPEND read_aux_SRCS readers/ufo-hdf5-reader.c common/hdf5.c)
        list(APPEND read_aux_LIBS ${HDF5_LIBRARIES})
        list(APPEND write_aux_SRCS writers/ufo-hdf5-writer.c common/hdf5.c)
        list(APPEND write_aux_LIBS ${HDF5_LIBRARIES})
        include_directories(${HDF5_INCLUDE_DIRS})
        link_directories(${HDF5_LIBRARY_DIRS})
    endif ()
endif ()

if (UCA_INCLUDE_DIRS AND UCA_LIBRARIES)
    list(APPEND ufofilter_SRCS ufo-camera-task.c)
    list(APPEND camera_aux_LIBS ${UCA_LIBRARIES})
    include_directories(${UCA_INCLUDE_DIRS})
    link_directories(${UCA_LIBRARY_DIRS})
endif ()

if (TARGET oclfft)
    option(WITH_OCLFFT "Use Apple FFT" ON)

    if (WITH_OCLFFT)
        list(APPEND fft_aux_LIBS oclfft)
        list(APPEND ifft_aux_LIBS oclfft)
        list(APPEND retrieve_phase_aux_LIBS oclfft)
        list(APPEND filter_aux_LIBS oclfft)
        set(HAVE_AMD OFF)
    endif ()
endif ()

if (CLFFT_FOUND)
    option(WITH_CLFFT "Use AMD clFFT" ON)

    if (WITH_CLFFT)
        include_directories(${CLFFT_INCLUDE_DIRS})
        list(APPEND fft_aux_LIBS ${CLFFT_LIBRARIES})
        list(APPEND ifft_aux_LIBS ${CLFFT_LIBRARIES})
        list(APPEND retrieve_phase_aux_LIBS ${CLFFT_LIBRARIES})
        list(APPEND filter_aux_LIBS ${CLFFT_LIBRARIES})
        set(HAVE_AMD ON)
    endif ()
endif ()

if (CLBLAST_FOUND)
    include_directories(${CLBLAST_INCLUDE_DIRS})
    list(APPEND ufofilter_SRCS ufo-gemm-task.c)
    list(APPEND gemm_aux_LIBS ${CLBLAST_LIBRARIES})
endif ()

if (PANGOCAIRO_FOUND)
    include_directories(${PANGOCAIRO_INCLUDE_DIRS})
    list(APPEND ufofilter_SRCS ufo-stamp-task.c)
    list(APPEND stamp_aux_LIBS ${PANGOCAIRO_LIBRARIES})
endif ()

if (OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
    list(APPEND ufofilter_SRCS ufo-cv-show-task.cpp)
    list(APPEND cv_show_aux_LIBS ${OpenCV_LIBRARIES})
    list(APPEND cv_show_aux_SRCS writers/ufo-writer.c)
elseif (OPENCV_FOUND)
    include_directories(${OPENCV_INCLUDE_DIRS})
    list(APPEND ufofilter_SRCS ufo-cv-show-task.cpp)
    list(APPEND cv_show_aux_LIBS ${OPENCV_LIBRARIES})
    list(APPEND cv_show_aux_SRCS writers/ufo-writer.c)
endif ()

if (ZMQ_FOUND AND JSON_GLIB_FOUND)
    include_directories(${ZMQ_INCLUDE_DIRS} ${JSON_GLIB_INCLUDE_DIRS})
    link_directories(${ZMQ_LIBRARY_DIRS} ${JSON_GLIB_LIBRARY_DIRS})
    list(APPEND ufofilter_SRCS ufo-zmq-pub-task.c ufo-zmq-sub-task.c)
    list(APPEND zmq_pub_aux_LIBS ${ZMQ_LIBRARIES} ${JSON_GLIB_LIBRARIES})
    list(APPEND zmq_sub_aux_LIBS ${ZMQ_LIBRARIES} ${JSON_GLIB_LIBRARIES})
endif ()
#}}}
#{{{ Plugin targets
include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${OpenCL_INCLUDE_DIRS}
                    ${UFO_INCLUDE_DIRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# build static auxiliary library first
add_library(ufoaux STATIC ${ufoaux_SRCS})

foreach(_src ${ufofilter_SRCS})
    # find plugin suffix
    string(REGEX REPLACE "ufo-([^ \\.]+)-task.*" "\\1" task "${_src}")

    # build string to get auxalleanous sources
    string(REPLACE "-" "_" _aux ${task})
    string(TOUPPER ${_aux} _aux_upper)

    # create an option name and add this to disable filters
    set(_aux_src "${_aux}_aux_SRCS")
    set(_aux_libs "${_aux}_aux_LIBS")

    set(target "ufofilter${task}")

    # build single shared library per filter
    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        add_library(${target} MODULE ${_src} ${${_aux_src}})
    else()
        add_library(${target} SHARED ${_src} ${${_aux_src}})
    endif()

    target_link_libraries(${target} ${ufofilter_LIBS} ${${_aux_libs}} ufoaux)

    list(APPEND all_targets ${target})

    install(TARGETS ${target}
            ARCHIVE DESTINATION ${UFO_PLUGINDIR}
            LIBRARY DESTINATION ${UFO_PLUGINDIR})
endforeach()
#}}}
#{{{ Subdirectories
add_subdirectory(kernels)
#}}}
