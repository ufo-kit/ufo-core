cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS 
    ufo.c
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    utils.c
    )

set(ufocore_HDRS
    ufo.h
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    utils.h
    )


# --- Find packages and libraries ---------------------------------------------
find_package(OpenCL)
find_package(FFTW3)


# --- Generate config.h -------------------------------------------------------
if (OPENCL_FOUND)
    set(HAVE_OPENCL TRUE)
    include_directories(${OPENCL_INCLUDE_DIRS})
endif(OPENCL_FOUND)

if (FFTW3_FOUND)
    set(HAVE_FFTW TRUE)
endif(FFTW3_FOUND)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


# --- Target ------------------------------------------------------------------
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
