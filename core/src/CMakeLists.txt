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
    ufo-resource-manager.h
    )

# --- Find packages and libraries ---------------------------------------------
pkg_check_modules(JSON_GLIB json-glib-1.0 REQUIRED)

set(ufocore_LIBS
    ${OPENCL_LIBRARIES}
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES}
    ${ETHOS_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    )


# --- Generate config.h -------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


# --- Target ------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${JSON_GLIB_INCLUDE_DIRS}
    )

add_library(ufo SHARED ${ufocore_SRCS})
target_link_libraries(ufo ${ufocore_LIBS})

install(TARGETS ufo 
    ARCHIVE DESTINATION lib 
    LIBRARY DESTINATION lib)

install(FILES ${ufocore_HDRS} DESTINATION include/ufo)

# --- install pkg-config file
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir      "\${exec_prefix}/lib")
set(includedir  "\${prefix}/include")

# FIXME: inside the ufo.pc.in we should set the lib names that we found out, not
# hard coded values
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ufo.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ufo.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ufo.pc DESTINATION lib/pkgconfig)
