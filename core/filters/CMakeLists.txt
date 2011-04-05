cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufofilter_SRCS 
    cpu/ufo-filter-hist.c
    )

# --- Find packages and libraries ---------------------------------------------

# --- Generate config.h -------------------------------------------------------

# --- Target ------------------------------------------------------------------
add_definitions("-std=c99 -Wall -Werror")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

foreach(_src ${ufofilter_SRCS})
    string(REGEX REPLACE "cpu/ufo-filter-([^ \\.]+).*" "\\1" result "${_src}")
    add_library(${result} SHARED ${_src})

    target_link_libraries(${result}
        ${GLIB2_LIBRARIES} 
        ${GOBJECT2_LIBRARIES}
        ${ETHOS_LIBRARIES}
        )
endforeach()

