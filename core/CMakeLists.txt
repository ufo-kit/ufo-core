cmake_minimum_required(VERSION 2.6)

# --- Find packages and libraries ---------------------------------------------

# These packages are required in all sub-directories because Glib and GObject is
# part of the API/ABI.

find_package(OpenCL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(GObjectIntrospection)
find_package(Vala)
pkg_check_modules(GLIB2 glib-2.0)
pkg_check_modules(GOBJECT2 gobject-2.0)
pkg_check_modules(ETHOS ethos-1.0)

include_directories(${CMAKE_CURRENT_BINARY_DIR}
    ${GLIB2_INCLUDE_DIRS}
    ${ETHOS_INCLUDE_DIRS}
    ${OPENCL_INCLUDE_DIRS}
    )

add_definitions("-std=c99 -Wall -Werror -fPIC")

add_subdirectory(src)
add_subdirectory(filters)
add_subdirectory(bindings)
add_subdirectory(doc)
add_subdirectory(tests)

