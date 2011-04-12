cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS 
    ufo-element.c
    ufo-container.c
    ufo-sequence.c
    ufo-split.c
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    ufo-connection.c
    ufo-resource-manager.c
    )

set(ufocore_HDRS
    ufo-element.h
    ufo-container.h
    ufo-sequence.h
    ufo-split.h
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    ufo-connection.h
    ufo-resource-manager.h
    )


# --- Find packages and libraries ---------------------------------------------
find_package(FFTW3)

pkg_check_modules(JSON_GLIB json-glib-1.0)


# --- Generate config.h -------------------------------------------------------
if (FFTW3_FOUND)
    set(HAVE_FFTW TRUE)
endif(FFTW3_FOUND)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


# --- Target ------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${JSON_GLIB_INCLUDE_DIRS}
    )

add_library(ufocore ${ufocore_SRCS})

target_link_libraries(ufocore 
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES}
    ${ETHOS_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    )

install(TARGETS ufocore 
    ARCHIVE DESTINATION lib 
    LIBRARY DESTINATION lib)

install(FILES ${ufocore_HDRS} DESTINATION include/ufo)
