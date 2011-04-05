cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS 
    ufo.c
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    ufo-plugin.c
    utils.c
    )

set(ufocore_HDRS
    ufo.h
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    ufo-plugin.h
    utils.h
    )


# --- Find packages and libraries ---------------------------------------------
find_package(OpenCL)
find_package(FFTW3)

pkg_check_modules(ETHOS ethos-1.0)


# --- Generate config.h -------------------------------------------------------
if (OPENCL_FOUND)
    set(HAVE_OPENCL TRUE)
    include_directories(${OPENCL_INCLUDE_DIRS})
endif(OPENCL_FOUND)

if (FFTW3_FOUND)
    set(HAVE_FFTW TRUE)
endif(FFTW3_FOUND)

find_path(OCLFFT_INCLUDE_DIR clFFT.h ${PROJECT_SOURCE_DIR}/thirdparty/oclfft)
if (OCLFFT_INCLUDE_DIR)
    set(HAVE_OCLFFT TRUE)
    include_directories(${OCLFFT_INCLUDE_DIR})
endif(OCLFFT_INCLUDE_DIR)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


# --- Target ------------------------------------------------------------------
add_definitions("-std=c99 -Wall -Werror")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(ufocore ${ufocore_SRCS})

target_link_libraries(ufocore 
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES}
    ${ETHOS_LIBRARIES}
    )

install(TARGETS ufocore 
    ARCHIVE DESTINATION lib 
    LIBRARY DESTINATION lib)

install(FILES ${ufocore_HDRS} DESTINATION include/ufo)
