cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS 
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    ufo-resource-manager.c
    )

set(ufocore_HDRS
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    ufo-resource-manager.h
    )


# --- Find packages and libraries ---------------------------------------------
find_package(FFTW3)


# --- Generate config.h -------------------------------------------------------
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
