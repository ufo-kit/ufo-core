cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufofilter_SRCS 
    cpu/ufo-filter-hist.c
    )

set(ufofilter_LIBS
    ufocore
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${ETHOS_LIBRARIES}
    )

# --- Find packages and libraries ---------------------------------------------
pkg_check_modules(UCA uca)

if (UCA_INCLUDE_DIRS AND UCA_LIBRARIES)
    set(ufofilter_SRCS ${ufofilter_SRCS} cpu/ufo-filter-uca.c)
    set(ufofilter_LIBS ${ufofilter_LIBS} ${UCA_LIBRARIES})
    include_directories(${UCA_INCLUDE_DIRS})
endif ()

# --- Generate config.h -------------------------------------------------------

# --- Target ------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})

foreach(_src ${ufofilter_SRCS})
    # find plugin suffix
    string(REGEX REPLACE "cpu/ufo-filter-([^ \\.]+).*" "\\1" _name "${_src}")

    # build single shared library per filter
    add_library(${_name}f SHARED ${_src})

    target_link_libraries(${_name}f ${ufofilter_LIBS})

    # configure the ethos plugin description
    set(UFO_PLUGIN_LIBRARY "lib${_name}f")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpu/${_name}.ufo-plugin.in
        ${CMAKE_CURRENT_BINARY_DIR}/${_name}.ufo-plugin)
endforeach()

