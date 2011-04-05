cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufofilter_SRCS 
    cpu/ufo-filter-hist.c
    )

# --- Find packages and libraries ---------------------------------------------

# --- Generate config.h -------------------------------------------------------

# --- Target ------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})

foreach(_src ${ufofilter_SRCS})
    # find plugin suffix
    string(REGEX REPLACE "cpu/ufo-filter-([^ \\.]+).*" "\\1" _name "${_src}")

    # build single shared library per filter
    add_library(${_name} SHARED ${_src})

    target_link_libraries(${_name}
        ${GLIB2_LIBRARIES} 
        ${GOBJECT2_LIBRARIES}
        ${ETHOS_LIBRARIES}
        ufocore
        )

    # configure the ethos plugin description
    set(UFO_PLUGIN_LIBRARY "lib${_name}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpu/${_name}.ufo-plugin.in
        ${CMAKE_CURRENT_BINARY_DIR}/${_name}.ufo-plugin)
endforeach()

