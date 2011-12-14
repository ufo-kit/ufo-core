cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS 
    ufo-channel.c
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    ufo-resource-manager.c
    )

set(ufocore_HDRS
    ufo-channel.h
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    ufo-resource-manager.h
    )

# --- Find packages and libraries ---------------------------------------------
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)

pkg_check_modules(JSON_GLIB json-glib-1.0 REQUIRED)

set(ufocore_LIBS
    ${OPENCL_LIBRARIES}
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES}
    ${ETHOS_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    )

# --- Add enum generation targets ---------------------------------------------
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
  COMMAND ${GLIB2_MKENUMS}
  ARGS
    --template ufo-enums.h.template
    ${ufocore_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${ufocore_HDRS}
          ${CMAKE_CURRENT_SOURCE_DIR}/ufo-enums.h.template
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c
  COMMAND ${GLIB2_MKENUMS}
  ARGS
    --template ufo-enums.c.template
    ${ufocore_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${ufocore_HDRS} ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
          ${CMAKE_CURRENT_SOURCE_DIR}/ufo-enums.c.template
)

set(ufocore_SRCS ${ufocore_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c)

# --- Target ------------------------------------------------------------------
get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

if (${LIB64} STREQUAL "TRUE" AND UNIX)
    set (LIB_SUFFIX 64)
else()
    set (LIB_SUFFIX "")
endif()

set(LIB_SUFFIX "")
set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${JSON_GLIB_INCLUDE_DIRS}
    )

add_library(ufo SHARED ${ufocore_SRCS})

set_target_properties(ufo PROPERTIES
    VERSION ${UFO_ABI_VERSION}
    SOVERSION ${UFO_ABI_MAJOR_VERSION})

target_link_libraries(ufo ${ufocore_LIBS})

install(TARGETS ufo 
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})

install(FILES 
    ${ufocore_HDRS} 
    ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
    DESTINATION include/ufo)

# --- install pkg-config file
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir      "${LIB_INSTALL_DIR}")
set(includedir  "\${prefix}/include")

# FIXME: inside the ufo.pc.in we should set the lib names that we found out, not
# hard coded values
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ufo.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ufo.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ufo.pc DESTINATION lib/pkgconfig)

# --- Generate config.h -------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_subdirectory(bindings)
