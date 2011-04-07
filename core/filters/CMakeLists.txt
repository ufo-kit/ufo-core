cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufofilter_SRCS 
    cpu/ufo-filter-hist.c
    cpu/ufo-filter-raw.c
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


# --- Target ------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})

foreach(_src ${ufofilter_SRCS})
    # find plugin suffix
    string(REGEX REPLACE "cpu/ufo-filter-([^ \\.]+).*" "\\1" filter "${_src}")

    set(target "filter${filter}")

    # build single shared library per filter
    add_library(${target} SHARED ${_src})

    target_link_libraries(${target} ${ufofilter_LIBS})

    # configure the ethos plugin description
    set(UFO_PLUGIN_LIBRARY "${target}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cpu/${filter}.ufo-plugin.in
        ${CMAKE_CURRENT_BINARY_DIR}/${filter}.ufo-plugin)
endforeach()

