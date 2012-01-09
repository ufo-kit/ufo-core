cmake_minimum_required(VERSION 2.6)
project(ufo)

set(TARNAME "libufo")
set(UFO_GIR_VERSION "0.1")
set(UFO_API_VERSION "0.1.0")
set(UFO_ABI_VERSION "0.1.0")
set(UFO_ABI_MAJOR_VERSION "0")

set(PACKAGE_VERSION "0.1.0")
set(PACKAGE_NAME "${TARNAME}")
set(PACKAGE_TARNAME "${TARNAME}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "http://ufo.kit.edu/ufo/newticket")

enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

# --- Options -----------------------------------------------------------------
option(WITH_PROFILING "Enable profiling" OFF)
if (WITH_PROFILING)
    add_definitions("-pg")
    set(CMAKE_C_FLAGS "-pg")
endif ()

# --- Find packages and libraries ---------------------------------------------

# These packages are required in all sub-directories because Glib and GObject is
# part of the API/ABI.

find_package(OpenCL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(GObjectIntrospection)
find_package(Vala)
pkg_check_modules(GLIB2 glib-2.0>=2.24 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.24 REQUIRED)
pkg_check_modules(GMODULE2 gmodule-2.0>=2.24 REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src
    ${GLIB2_INCLUDE_DIRS}
    ${ETHOS_INCLUDE_DIRS}
    ${OPENCL_INCLUDE_DIRS}
    )

add_definitions("-std=c99 -Wall -fPIC")
add_definitions(-DG_LOG_DOMAIN="Ufo")

add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(tests)

