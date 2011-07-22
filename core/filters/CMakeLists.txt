cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufofilter_SRCS 
    ufo-filter-backproject.c
    ufo-filter-center-of-rotation.c
    ufo-filter-circle-crop.c
    ufo-filter-cl.c
    ufo-filter-filter.c
    ufo-filter-hist.c
    ufo-filter-normalize.c
    ufo-filter-null.c
    ufo-filter-raw.c
    ufo-filter-scale.c
    ufo-filter-sino-generator.c
    ufo-filter-stripe-removal.c
    )

set(ufofilter_KERNELS
    backproject.cl
    scale.cl
    fft.cl
    filter.cl
    )

set(ufofilter_LIBS
    ufo
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${ETHOS_LIBRARIES}
    )


# --- Add filters that depend on other libraries ------------------------------
find_package(TIFF)
find_package(OCLFFT)
pkg_check_modules(UCA uca)
pkg_check_modules(OPENCV opencv)

if (UCA_INCLUDE_DIRS AND UCA_LIBRARIES)
    set(ufofilter_SRCS ${ufofilter_SRCS} ufo-filter-uca.c)
    set(ufofilter_LIBS ${ufofilter_LIBS} ${UCA_LIBRARIES})
    include_directories(${UCA_INCLUDE_DIRS})
endif ()

if (TIFF_FOUND)
    set(ufofilter_SRCS ${ufofilter_SRCS} ufo-filter-reader.c)
    set(ufofilter_SRCS ${ufofilter_SRCS} ufo-filter-writer.c)
    set(ufofilter_LIBS ${ufofilter_LIBS} ${TIFF_LIBRARIES})
    include_directories(${TIFF_INCLUDE_DIRS})
endif ()

if (OCLFFT_FOUND)
    set(ufofilter_SRCS ${ufofilter_SRCS} ufo-filter-fft.c)
    set(ufofilter_SRCS ${ufofilter_SRCS} ufo-filter-ifft.c)
    set(ufofilter_LIBS ${ufofilter_LIBS} ${OCLFFT_LIBRARIES})
    include_directories(${OCLFFT_INCLUDE_DIRS})
endif ()

if (OPENCV_FOUND)
    set(ufofilter_SRCS ${ufofilter_SRCS} ufo-filter-cv-show.c)
    set(ufofilter_LIBS ${ufofilter_LIBS} ${OPENCV_LIBRARIES})
    include_directories(${OPENCV_INCLUDE_DIRS})
endif ()


# --- Target ------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})

foreach(_src ${ufofilter_SRCS})
    # find plugin suffix
    string(REGEX REPLACE "ufo-filter-([^ \\.]+).*" "\\1" filter "${_src}")

    set(target "filter${filter}")

    # build single shared library per filter
    add_library(${target} SHARED ${_src})

    target_link_libraries(${target} ${ufofilter_LIBS})

    # configure the ethos plugin description
    set(UFO_PLUGIN_LIBRARY "${target}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${filter}.ufo-plugin.in
        ${CMAKE_CURRENT_BINARY_DIR}/${filter}.ufo-plugin)
endforeach()

# for now we just copy the kernels into the tests/ dir
foreach(_kernel ${ufofilter_KERNELS})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${_kernel}
        ${CMAKE_CURRENT_BINARY_DIR}/../tests/${_kernel})
endforeach()
